// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/internal/test_composebox_query_controller.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace signin {
class IdentityManager;
}

class FakeVariationsClient;
class KeyedService;
class TemplateURLService;

std::unique_ptr<KeyedService> BuildMockContextualSearchServiceInstance(
    content::BrowserContext* context);

class MockQueryController
    : public contextual_search::TestComposeboxQueryController {
 public:
  MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      std::unique_ptr<
          contextual_search::ContextualSearchContextController::ConfigParams>
          query_controller_config_params);
  ~MockQueryController() override;

  MOCK_METHOD(void, InitializeIfNeeded, (), (override));
  MOCK_METHOD(void, TriggerFetchClusterInfo, (), (override));
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input,
               std::optional<lens::ImageEncodingOptions> image_options),
              (override));
  MOCK_METHOD(void,
              CreateSearchUrl,
              (std::unique_ptr<CreateSearchUrlRequestInfo>
                   search_url_request_info,
               base::OnceCallback<void(GURL)> callback),
              (override));
  MOCK_METHOD(bool, DeleteFile, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, ClearFiles, (), (override));
  MOCK_METHOD(const contextual_search::FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken& file_token),
              (override));

  const contextual_search::FileInfo* FakeGetFileInfo(
      const base::UnguessableToken& file_token);

  void FakeStartFileUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input,
      std::optional<lens::ImageEncodingOptions> image_options);

  void FakeCreateSearchUrl(
      std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
      base::OnceCallback<void(GURL)> callback);

  void AddFileInfoForTesting(const base::UnguessableToken& file_token,
                             lens::MimeType mime_type) {
    auto file_info = std::make_unique<contextual_search::FileInfo>();
    file_info->file_token = file_token;
    file_info->mime_type = mime_type;
    files_[file_token] = std::move(file_info);
  }

  void InitializeIfNeededBase() {
    TestComposeboxQueryController::InitializeIfNeeded();
  }

  void TriggerFetchClusterInfoBase() {
    TestComposeboxQueryController::TriggerFetchClusterInfo();
  }

  void CreateSearchUrlBase(std::unique_ptr<CreateSearchUrlRequestInfo>
                               search_url_request_info,
                           base::OnceCallback<void(GURL)> callback) {
    TestComposeboxQueryController::CreateSearchUrl(
        std::move(search_url_request_info), std::move(callback));
  }

  void AddObserver(contextual_search::ContextualSearchContextController::
                       ContextUploadStatusObserver* obs) override {
    observers_.AddObserver(obs);
    TestComposeboxQueryController::AddObserver(obs);
  }

  void RemoveObserver(contextual_search::ContextualSearchContextController::
                          ContextUploadStatusObserver* obs) override {
    observers_.RemoveObserver(obs);
    TestComposeboxQueryController::RemoveObserver(obs);
  }

  void NotifySuccess(const base::UnguessableToken& file_token,
                     lens::MimeType mime_type = lens::MimeType::kHtml) {
    for (auto& observer : observers_) {
      observer.OnContextUploadStatusChanged(
          file_token, mime_type,
          contextual_search::ContextUploadStatus::kUploadSuccessful,
          std::nullopt);
    }
  }

 private:
  base::ObserverList<contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver>
      observers_;
  std::map<base::UnguessableToken, std::unique_ptr<contextual_search::FileInfo>>
      files_;
  base::WeakPtrFactory<MockQueryController> weak_ptr_factory_{this};
};

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  // WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
};

class MockContextualSearchMetricsRecorder
    : public contextual_search::ContextualSearchMetricsRecorder {
 public:
  MockContextualSearchMetricsRecorder();
  ~MockContextualSearchMetricsRecorder() override;

  MOCK_METHOD(void,
              NotifySessionStateChanged,
              (contextual_search::SessionState session_state),
              (override));
  MOCK_METHOD(void,
              NotifyQuerySubmitted,
              (bool has_tab_context,
               bool has_non_tab_context,
               int query_text_length,
               int file_count,
               bool has_drive_context),
              (override));
  MOCK_METHOD(void, ActivateMetricsFunnel, (const std::string&), (override));
  MOCK_METHOD(void, RecordToolMode, (omnibox::ToolMode tool_mode), (override));
  MOCK_METHOD(void,
              RecordModelMode,
              (omnibox::ModelMode model_mode),
              (override));
  MOCK_METHOD(void,
              RecordModesOnSubmission,
              (omnibox::ToolMode tool_mode,
               omnibox::ModelMode model_mode,
               const std::vector<omnibox::InputType>& input_types),
              (override));
  MOCK_METHOD(void, RecordZeroSuggestClick, (bool is_contextual), (override));
  MOCK_METHOD(void,
              RecordTypedSuggestNavigation,
              (bool is_verbatim),
              (override));

  void NotifySessionStateChangedBase(
      contextual_search::SessionState session_state) {
    ContextualSearchMetricsRecorder::NotifySessionStateChanged(session_state);
  }

  void NotifyQuerySubmittedBase(bool has_tab_context,
                                bool has_non_tab_context,
                                int query_text_length,
                                int file_count,
                                bool has_drive_context) {
    ContextualSearchMetricsRecorder::NotifyQuerySubmitted(
        has_tab_context, has_non_tab_context, query_text_length, file_count,
        has_drive_context);
  }

  void RecordToolModeBase(omnibox::ToolMode tool_mode) {
    ContextualSearchMetricsRecorder::RecordToolMode(tool_mode);
  }

  void RecordModelModeBase(omnibox::ModelMode model_mode) {
    ContextualSearchMetricsRecorder::RecordModelMode(model_mode);
  }

  void RecordModesOnSubmissionBase(
      omnibox::ToolMode tool_mode,
      omnibox::ModelMode model_mode,
      const std::vector<omnibox::InputType>& input_types) {
    ContextualSearchMetricsRecorder::RecordModesOnSubmission(
        tool_mode, model_mode, input_types);
  }

  void RecordZeroSuggestClickBase(bool is_contextual) {
    ContextualSearchMetricsRecorder::RecordZeroSuggestClick(is_contextual);
  }

  void RecordTypedSuggestNavigationBase(bool is_verbatim) {
    ContextualSearchMetricsRecorder::RecordTypedSuggestNavigation(is_verbatim);
  }
};

class ContextualSearchboxHandlerTestHarness
    : public ChromeRenderViewHostTestHarness {
 public:
  ContextualSearchboxHandlerTestHarness();
  ~ContextualSearchboxHandlerTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  TemplateURLService* template_url_service() { return template_url_service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return shared_url_loader_factory_;
  }
  FakeVariationsClient* fake_variations_client() {
    return fake_variations_client_.get();
  }

  ntp_composebox::FeatureConfig& scoped_config() {
    return scoped_config_.Get();
  }
  TestingProfile::TestingFactories GetTestingFactories() const override;

 private:
  ntp_composebox::ScopedFeatureConfigForTesting scoped_config_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  base::HistogramTester histogram_tester_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_
