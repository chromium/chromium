// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"

#include <optional>

#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

std::unique_ptr<KeyedService> BuildMockContextualSearchServiceInstance(
    content::BrowserContext* /*context*/) {
  auto mock_service =
      std::make_unique<contextual_search::MockContextualSearchService>(
          /*identity_manager=*/nullptr,
          /*url_loader_factory=*/nullptr,
          /*template_url_service=*/nullptr,
          /*variations_client=*/nullptr, version_info::Channel::UNKNOWN,
          "en-US");

  ON_CALL(*mock_service, CreateSession)
      .WillByDefault(
          [service_ptr = mock_service.get()](
              std::unique_ptr<
                  contextual_search::ContextualSearchContextController::
                      ConfigParams> params,
              contextual_search::ContextualSearchSource source,
              std::optional<lens::LensOverlayInvocationSource>
                  invocation_source) {
            auto query_controller = std::make_unique<MockQueryController>(
                /*identity_manager=*/nullptr, /*url_loader_factory=*/nullptr,
                version_info::Channel::UNKNOWN, "en-US",
                /*template_url_service=*/nullptr,
                /*variations_client=*/nullptr, std::move(params));

            auto* query_controller_ptr = query_controller.get();

            ON_CALL(*query_controller_ptr, GetFileInfo)
                .WillByDefault(
                    testing::Invoke(query_controller_ptr,
                                    &MockQueryController::FakeGetFileInfo));
            ON_CALL(*query_controller_ptr, StartFileUploadFlow)
                .WillByDefault(testing::Invoke(
                    query_controller_ptr,
                    &MockQueryController::FakeStartFileUploadFlow));
            ON_CALL(*query_controller_ptr, CreateSearchUrl)
                .WillByDefault(
                    testing::Invoke(query_controller_ptr,
                                    &MockQueryController::FakeCreateSearchUrl));

            auto metrics_recorder =
                std::make_unique<MockContextualSearchMetricsRecorder>();

            return service_ptr->CreateSessionForTesting(
                std::move(query_controller), std::move(metrics_recorder));
          });

  return std::move(mock_service);
}

MockQueryController::MockQueryController(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    std::string locale,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    std::unique_ptr<
        contextual_search::ContextualSearchContextController::ConfigParams>
        query_controller_config_params)
    : TestComposeboxQueryController(identity_manager,
                                    url_loader_factory,
                                    channel,
                                    locale,
                                    template_url_service,
                                    variations_client,
                                    std::move(query_controller_config_params),
                                    /*enable_cluster_info_ttl=*/false) {
  ON_CALL(*this, CreateSearchUrl)
      .WillByDefault(testing::Invoke(
          this, &MockQueryController::CreateSearchUrlBase));
}
MockQueryController::~MockQueryController() = default;

const contextual_search::FileInfo* MockQueryController::FakeGetFileInfo(
    const base::UnguessableToken& file_token) {
  auto it = files_.find(file_token);
  if (it != files_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void MockQueryController::FakeStartFileUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input,
    std::optional<lens::ImageEncodingOptions> image_options) {
  lens::MimeType mime_type = lens::MimeType::kHtml;
  if (contextual_input && contextual_input->primary_content_type) {
    mime_type = contextual_input->primary_content_type.value();
  }
  AddFileInfoForTesting(file_token, mime_type);
  TriggerFetchClusterInfo();

  // Post a task to notify success asynchronously. This ensures that the
  // frontend receives the Mojo response containing the fileToken *before* the
  // upload status event arrives, preventing the event from being dropped.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQueryController::NotifySuccess,
                     weak_ptr_factory_.GetWeakPtr(), file_token, mime_type));
}

void MockQueryController::FakeCreateSearchUrl(
    std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
    base::OnceCallback<void(GURL)> callback) {
  std::string query = search_url_request_info->query_text;
  base::ReplaceChars(query, " ", "+", &query);
  GURL result_url("https://www.google.com/search?q=" + query);
  if (search_url_request_info->search_url_type == SearchUrlType::kAim) {
    result_url = net::AppendOrReplaceQueryParameter(result_url, "udm", "50");
  }
  std::move(callback).Run(result_url);
}

content::WebContents* TestWebContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
}

MockContextualSearchMetricsRecorder::MockContextualSearchMetricsRecorder()
    : ContextualSearchMetricsRecorder(
          contextual_search::ContextualSearchSource::kNewTabPage) {}
MockContextualSearchMetricsRecorder::~MockContextualSearchMetricsRecorder() =
    default;

ContextualSearchboxHandlerTestHarness::ContextualSearchboxHandlerTestHarness()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
ContextualSearchboxHandlerTestHarness::
    ~ContextualSearchboxHandlerTestHarness() = default;

void ContextualSearchboxHandlerTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_factory_);

  // Set a default search provider for `template_url_service_`
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(template_url_service_);
  template_url_service_->Load();
  fake_variations_client_ = std::make_unique<FakeVariationsClient>();
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

  auto* image_upload =
      scoped_config_.Get().config.mutable_composebox()->mutable_image_upload();
  image_upload->set_downscale_max_image_size(1000000);
  image_upload->set_downscale_max_image_width(1000);
  image_upload->set_downscale_max_image_height(1000);
  image_upload->set_image_compression_quality(30);
}

void ContextualSearchboxHandlerTestHarness::TearDown() {
  template_url_service_ = nullptr;
  ChromeRenderViewHostTestHarness::TearDown();
}

TestingProfile::TestingFactories
ContextualSearchboxHandlerTestHarness::GetTestingFactories() const {
  return TestingProfile::TestingFactories{
      TestingProfile::TestingFactory{
          TemplateURLServiceFactory::GetInstance(),
          base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
      TestingProfile::TestingFactory{
          ContextualSearchServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* /*context*/)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<contextual_search::ContextualSearchService>(
                /*identity_manager=*/nullptr,
                /*url_loader_factory=*/nullptr,
                /*template_url_service=*/nullptr,
                /*variations_client=*/nullptr, version_info::Channel::UNKNOWN,
                "en-US");
          })}};
}
