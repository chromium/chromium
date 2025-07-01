// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/version_info/channel.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockQueryController : public ComposeboxQueryController {
 public:
  explicit MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service)
      : ComposeboxQueryController(identity_manager,
                                  url_loader_factory,
                                  channel,
                                  locale,
                                  template_url_service) {}
  ~MockQueryController() override = default;

  MOCK_METHOD(void, NotifySessionStarted, ());
  MOCK_METHOD(void, NotifySessionAbandoned, ());
  MOCK_METHOD(
      void,
      StartFileUploadFlow,
      (std::unique_ptr<ComposeboxQueryController::FileInfo> file_info_mojom,
       scoped_refptr<base::RefCountedBytes> file_data));
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
          navigation_handle_callback) override {
    source->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));
    return source;
  }
};

class ComposeboxHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  ComposeboxHandlerTest() = default;
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    // Set a default search provider for `template_url_service_`
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service_);
    template_url_service_->Load();
    TemplateURLData data;
    data.SetShortName(u"Google");
    data.SetKeyword(u"google.com");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, shared_url_loader_factory_,
        version_info::Channel::UNKNOWN, "en-US", template_url_service_);
    query_controller_ = query_controller_ptr.get();
    web_contents()->SetDelegate(&delegate_);
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler>(),
        std::move(query_controller_ptr), web_contents());
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }
  TemplateURLService& template_url_service() { return *template_url_service_; }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return TestingProfile::TestingFactory{
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)};
  }

  void TearDown() override {
    template_url_service_ = nullptr;
    query_controller_ = nullptr;
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  std::unique_ptr<ComposeboxHandler> handler_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<MockQueryController> query_controller_;
  TestWebContentsDelegate delegate_;
  raw_ptr<TemplateURLService> template_url_service_;
};

TEST_F(ComposeboxHandlerTest, NotifySessionStarted) {
  EXPECT_CALL(query_controller(), NotifySessionStarted).Times(1);
  handler().NotifySessionStarted();
}

TEST_F(ComposeboxHandlerTest, NotifySessionAbandoned) {
  EXPECT_CALL(query_controller(), NotifySessionAbandoned).Times(1);
  handler().NotifySessionAbandoned();
}

TEST_F(ComposeboxHandlerTest, SubmitQuery) {
  const std::string query = "test";
  content::TestNavigationObserver navigation_observer(web_contents());
  handler().SubmitQuery(query, 1, false, false, false, false);
  auto navigation = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  ASSERT_TRUE(navigation);
  navigation->Commit();
  navigation_observer.Wait();

  GURL expected_url = query_controller().CreateAimUrl(query);

  // Ensure navigation occurred.
  EXPECT_EQ(expected_url,
            web_contents()->GetController().GetLastCommittedEntry()->GetURL());
}

TEST_F(ComposeboxHandlerTest, AddFile_Pdf) {
  composebox::mojom::SelectedFileInfoPtr file_info =
      composebox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/pdf";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(1);
  handler().AddFile(std::move(file_info), std::move(file_data));
}

TEST_F(ComposeboxHandlerTest, AddFile_Image) {
  composebox::mojom::SelectedFileInfoPtr file_info =
      composebox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.png";
  file_info->selection_time = base::Time::Now();
  file_info->mime_type = "application/image";

  std::vector<uint8_t> test_data = {1, 2, 3, 4};
  auto test_data_span = base::span<const uint8_t>(test_data);
  mojo_base::BigBuffer file_data(test_data_span);

  EXPECT_CALL(query_controller(), StartFileUploadFlow).Times(1);
  handler().AddFile(std::move(file_info), std::move(file_data));
}
