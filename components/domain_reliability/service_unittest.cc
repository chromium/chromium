// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/service.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"

namespace domain_reliability {

namespace {

class TestPermissionManager : public content::PermissionControllerDelegate {
 public:
  TestPermissionManager() : get_permission_status_count_(0) {}

  int get_permission_status_count() const {
    return get_permission_status_count_;
  }
  content::PermissionType last_permission() const { return last_permission_; }
  const GURL& last_requesting_origin() const { return last_requesting_origin_; }
  const GURL& last_embedding_origin() const { return last_embedding_origin_; }

  void set_permission_status(blink::mojom::PermissionStatus permission_status) {
    permission_status_ = permission_status;
  }

  // content::PermissionManager:

  ~TestPermissionManager() override {}

  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override {
    ++get_permission_status_count_;

    last_permission_ = permission;
    last_requesting_origin_ = requesting_origin;
    last_embedding_origin_ = embedding_origin;

    return permission_status_;
  }

  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override {
    return GetPermissionStatus(
        permission, requesting_origin,
        content::WebContents::FromRenderFrameHost(render_frame_host)
            ->GetLastCommittedURL()
            .GetOrigin());
  }

  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override {
    NOTIMPLEMENTED();
    return content::PermissionController::kNoPendingOperation;
  }

  int RequestPermissions(
      const std::vector<content::PermissionType>& permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<
          void(const std::vector<blink::mojom::PermissionStatus>&)>& callback)
      override {
    NOTIMPLEMENTED();
    return content::PermissionController::kNoPendingOperation;
  }

  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override {
    NOTIMPLEMENTED();
  }

  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override {
    NOTIMPLEMENTED();
    return 0;
  }

  void UnsubscribePermissionStatusChange(int subscription_id) override {
    NOTIMPLEMENTED();
  }

 private:
  // Number of calls made to GetPermissionStatus.
  int get_permission_status_count_;

  // Parameters to last call to GetPermissionStatus:

  content::PermissionType last_permission_;
  GURL last_requesting_origin_;
  GURL last_embedding_origin_;

  // Value to return from GetPermissionStatus.
  blink::mojom::PermissionStatus permission_status_;

  DISALLOW_COPY_AND_ASSIGN(TestPermissionManager);
};

}  // namespace

class DomainReliabilityServiceTest : public testing::Test {
 protected:
  using RequestInfo = DomainReliabilityMonitor::RequestInfo;

  DomainReliabilityServiceTest()
      : upload_reporter_string_("test"),
        permission_manager_(new TestPermissionManager()) {
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI});
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO});
    url_request_context_getter_ =
        new net::TestURLRequestContextGetter(network_task_runner);
    browser_context_.SetPermissionControllerDelegate(
        base::WrapUnique(permission_manager_));
    service_ = base::WrapUnique(DomainReliabilityService::Create(
        upload_reporter_string_, &browser_context_));
    monitor_ = service_->CreateMonitor(ui_task_runner, network_task_runner);
    monitor_->MoveToNetworkThread();
    // Let the NetworkConnectionTracker registration complete.
    thread_bundle_.RunUntilIdle();
    monitor_->InitURLRequestContext(url_request_context_getter_);
    monitor_->SetDiscardUploads(true);
  }

  ~DomainReliabilityServiceTest() override {
    if (monitor_)
      monitor_->Shutdown();
  }

  void OnRequestLegComplete(RequestInfo request) {
    monitor_->OnRequestLegComplete(request);
  }

  int GetDiscardedUploadCount() const {
    return monitor_->uploader_->GetDiscardedUploadCount();
  }

  content::TestBrowserThreadBundle thread_bundle_;

  std::string upload_reporter_string_;

  content::TestBrowserContext browser_context_;

  // Owned by browser_context_, not the test class.
  TestPermissionManager* permission_manager_;

  std::unique_ptr<DomainReliabilityService> service_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<DomainReliabilityMonitor> monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityServiceTest);
};

namespace {

TEST_F(DomainReliabilityServiceTest, Create) {}

TEST_F(DomainReliabilityServiceTest, UploadAllowed) {
  permission_manager_->set_permission_status(
      blink::mojom::PermissionStatus::GRANTED);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(1, permission_manager_->get_permission_status_count());

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_EQ(1, GetDiscardedUploadCount());
}

TEST_F(DomainReliabilityServiceTest, UploadForbidden) {
  permission_manager_->set_permission_status(
      blink::mojom::PermissionStatus::DENIED);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(1, permission_manager_->get_permission_status_count());

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_EQ(0, GetDiscardedUploadCount());
}

TEST_F(DomainReliabilityServiceTest, MonitorDestroyedBeforeCheckRuns) {
  permission_manager_->set_permission_status(
      blink::mojom::PermissionStatus::DENIED);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  monitor_->Shutdown();
  monitor_.reset();

  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(1, permission_manager_->get_permission_status_count());

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  // Makes no sense to check upload count, since monitor was destroyed.
}

TEST_F(DomainReliabilityServiceTest, MonitorDestroyedBeforeCheckReturns) {
  permission_manager_->set_permission_status(
      blink::mojom::PermissionStatus::DENIED);

  monitor_->AddContextForTesting(
      MakeTestConfigWithOrigin(GURL("https://example/")));

  RequestInfo request;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_REFUSED);
  request.response_info.socket_address =
      net::HostPortPair::FromString("1.2.3.4");
  request.url = GURL("https://example/");
  request.response_info.was_cached = false;
  request.response_info.network_accessed = true;
  request.response_info.was_fetched_via_proxy = false;
  request.load_flags = 0;
  request.load_timing_info.request_start = base::TimeTicks::Now();
  request.upload_depth = 0;

  OnRequestLegComplete(request);

  monitor_->ForceUploadsForTesting();

  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(1, permission_manager_->get_permission_status_count());

  monitor_->Shutdown();
  monitor_.reset();

  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  // Makes no sense to check upload count, since monitor was destroyed.
}

}  // namespace

}  // namespace domain_reliability
