// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

struct EvalJsResult;

}  // namespace content

namespace media_router {

// Base class that generates an access code cast dialog and all objects that are
// required for interacting with the dialog.
class AccessCodeCastIntegrationBrowserTest
    : public SupportsTestDialog<MixinBasedInProcessBrowserTest> {
 public:
  AccessCodeCastIntegrationBrowserTest();
  ~AccessCodeCastIntegrationBrowserTest() override;

  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;

  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Makes user signed-in with the stub account's email and sets the
  // |consent_level| for that account.
  void SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel consent_level,
                                           Profile* profile,
                                           bool sign_in_account = true);

  void EnableAccessCodeCasting();

  content::WebContents* ShowDialog();

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override;

  void CloseDialog(content::WebContents* dialog_contents);

  void SetAccessCode(std::string access_code,
                     content::WebContents* dialog_contents);
  void PressSubmit(content::WebContents* dialog_contents);
  void PressSubmitAndWaitForClose(content::WebContents* dialog_contents);

  void SetAccessCodeUsingKeyPress(const std::string& access_code);
  void PressSubmitUsingKeyPress();
  void PressSubmitAndWaitForCloseUsingKeyPress(
      content::WebContents* dialog_contents);
  void CloseDialogUsingKeyPress();

  // This function spins the run loop until an error code is surfaced.
  int WaitForAddSinkErrorCode(content::WebContents* dialog_contents);

  bool HasSinkInDevicesDict(const MediaSink::Id& sink_id);
  std::optional<base::Time> GetDeviceAddedTimeFromDict(
      const MediaSink::Id& sink_id);

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::unique_ptr<KeyedService> CreateAccessCodeCastSinkService(
      content::BrowserContext* context);

  // This function should be called once.
  MockCastMediaSinkServiceImpl* CreateCastMediaSinkServiceImpl();

  MockCastMediaSinkServiceImpl* CreateImpl();

  void SpinRunLoop(base::TimeDelta delay);

  std::string GetElementScript() {
    return std::string(
        R"(document.getElementsByTagName('access-code-cast-app')[0])");
  }

  std::string GetErrorElementScript() {
    return std::string(GetElementScript() + ".$['errorMessage']");
  }

  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params);

  void SetEndpointFetcherMockResponse(const std::string& response_data,
                                      net::HttpStatusCode response_code,
                                      net::Error error);

  void SetMockOpenChannelCallbackResponse(bool channel_opened);
  void UpdateSinks(const std::vector<MediaSink>& sinks,
                   const std::vector<url::Origin>& origins);
  void UpdateRoutes(const std::vector<MediaRoute>& routes);

  void ExpectStartRouteCallFromTabMirroring(
      const std::string& sink_name,
      const std::string& media_source_id,
      content::WebContents* web_contents,
      base::TimeDelta timeout = base::Seconds(60),
      media_router::MockMediaRouter* media_router = nullptr);

  void ExpectMediaRouterHasNoSinks(base::OnceClosure callback, bool has_sink);
  void ExpectMediaRouterHasSink(base::OnceClosure callback, bool has_sink);

  void MockOnChannelOpenedCall(const MediaSinkInternal& cast_sink,
                               std::unique_ptr<net::BackoffEntry> backoff_entry,
                               CastDeviceCountMetrics::SinkSource sink_source,
                               ChannelOpenedCallback callback,
                               cast_channel::CastSocketOpenParams open_params);

  AccessCodeCastPrefUpdater* GetPrefUpdater();

  void AddScreenplayTag(const std::string& screenplay_tag);

  MockCastMediaSinkServiceImpl* mock_cast_media_sink_service_impl() {
    return impl_;
  }

  syncer::TestSyncService* sync_service(Profile* profile) {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile));
  }

  static constexpr char kAccessCodeCastNewDeviceScreenplayTag[] =
      "screenplay-a7ecd49d-f138-40b0-a830-3c1ebb4f4c5a";
  static constexpr char kAccessCodeCastSavedDeviceScreenplayTag[] =
      "screenplay-5aba818e-1cca-4c41-811a-4bf704cbe820";

  base::Time device_added_time() { return device_added_time_; }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() {
    return task_runner_;
  }

  void UpdateDeviceAddedTime(const MediaSink::Id& sink_id);
  void SetAccessCodeCastSinkServiceTaskRunner();
  bool IsAccessCodeCastLacrosSyncEnabled();

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

 protected:
  raw_ptr<media_router::MockMediaRouter, DanglingUntriaged> media_router_ =
      nullptr;
  std::vector<raw_ptr<MediaSinksObserver, VectorExperimental>>
      media_sinks_observers_;
  std::vector<raw_ptr<media_router::MediaRoutesObserver, VectorExperimental>>
      media_routes_observers_;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  content::EvalJsResult EvalJs(const std::string& string_value,
                               content::WebContents* web_contents);

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  std::string url_to_intercept_;
  bool should_intercept_response_ = false;

  std::unique_ptr<cast_channel::MockCastSocketService,
                  base::OnTaskRunnerDeleter>
      mock_cast_socket_service_;
  raw_ptr<MockCastMediaSinkServiceImpl> impl_ = nullptr;

  std::unique_ptr<TestMediaSinkService> mock_dual_media_sink_service_;

  net::Error error_;
  net::HttpStatusCode response_code_;
  std::string response_data_;

  bool open_channel_response_ = true;
  std::set<MediaSink::Id> added_sink_ids_ = {};

  mojom::RouteRequestResultCode result_code_ =
      mojom::RouteRequestResultCode::OK;

  base::Time device_added_time_;

  base::WeakPtrFactory<AccessCodeCastIntegrationBrowserTest> weak_ptr_factory_{
      this};
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_
