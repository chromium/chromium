// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"

#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"

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
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<contextual_search::ContextualSearchService>(
                /*identity_manager=*/nullptr,
                /*url_loader_factory=*/nullptr,
                /*template_url_service=*/nullptr,
                /*variations_client=*/nullptr, version_info::Channel::UNKNOWN,
                "en-US");
          })}};
}
