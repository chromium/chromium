// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/test_renderer_host.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class ContentLoFiUIServiceTest : public content::RenderViewHostTestHarness {
 public:
  ContentLoFiUIServiceTest()
      : content::RenderViewHostTestHarness(
            content::TestBrowserThreadBundle::DEFAULT),
        callback_called_(false) {}

  void RunTestOnIOThread(base::RunLoop* ui_run_loop) {
    ASSERT_TRUE(ui_run_loop);
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    net::TestURLRequestContext context(true);
    net::MockClientSocketFactory mock_socket_factory;
    net::TestDelegate delegate;
    context.set_client_socket_factory(&mock_socket_factory);
    context.Init();

    content_lofi_ui_service_.reset(new ContentLoFiUIService(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI}),
        base::Bind(&ContentLoFiUIServiceTest::OnLoFiResponseReceivedCallback,
                   base::Unretained(this))));

    std::unique_ptr<net::URLRequest> request =
        CreateRequest(context, &delegate);

    content_lofi_ui_service_->OnLoFiReponseReceived(*request);

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&base::RunLoop::Quit, base::Unretained(ui_run_loop)));
  }

  std::unique_ptr<net::URLRequest> CreateRequest(
      const net::TestURLRequestContext& context,
      net::TestDelegate* delegate) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    std::unique_ptr<net::URLRequest> request =
        context.CreateRequest(GURL("http://www.google.com/"), net::IDLE,
                              delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_SUB_FRAME, nullptr,
        web_contents()->GetMainFrame()->GetProcess()->GetID(), -1,
        web_contents()->GetMainFrame()->GetRoutingID(),
        /*is_main_frame=*/false,
        /*allow_download=*/false,
        /*is_async=*/false, content::SERVER_LOFI_ON,
        /*navigation_ui_data*/ nullptr);

    return request;
  }

  void OnLoFiResponseReceivedCallback(content::WebContents* web_contents) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    callback_called_ = true;
  }

  void VerifyOnLoFiResponseReceivedCallback() {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    EXPECT_TRUE(callback_called_);
  }

 private:
  std::unique_ptr<ContentLoFiUIService> content_lofi_ui_service_;
  bool callback_called_;
};

TEST_F(ContentLoFiUIServiceTest, OnLoFiResponseReceived) {
  base::RunLoop ui_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ContentLoFiUIServiceTest::RunTestOnIOThread,
                     base::Unretained(this), &ui_run_loop));
  ui_run_loop.Run();
  base::RunLoop().RunUntilIdle();
  VerifyOnLoFiResponseReceivedCallback();
}

}  // namespace data_reduction_proxy
