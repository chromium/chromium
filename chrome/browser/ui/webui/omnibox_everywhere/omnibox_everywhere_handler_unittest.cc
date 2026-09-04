// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_web_ui.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kVoiceSearchQueryParameterKey[] = "gs_ivs";
constexpr char kVoiceSearchQueryParameterValue[] = "1";

class MockOmniboxEverywhereService : public OmniboxEverywhereService {
 public:
  explicit MockOmniboxEverywhereService(Profile* profile)
      : OmniboxEverywhereService(profile) {}
  ~MockOmniboxEverywhereService() override = default;

  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url,
               WindowOpenDisposition disposition,
               ui::PageTransition transition,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback),
              (override));
  MOCK_METHOD(void, ShowProfilePicker, (), (override));
  MOCK_METHOD(void, OnDrivePickerOpened, (), (override));
  MOCK_METHOD(void, OnDrivePickerClosed, (), (override));
};

class OmniboxEverywhereHandlerPublic : public OmniboxEverywhereHandler {
 public:
  using OmniboxEverywhereHandler::OmniboxEverywhereHandler;
  using SearchboxHandler::CreateAutocompleteMatch;
};

class OmniboxEverywhereHandlerTest
    : public ContextualSearchboxHandlerTestHarness {
 public:
  OmniboxEverywhereHandlerTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }
  ~OmniboxEverywhereHandlerTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    auto factories =
        ContextualSearchboxHandlerTestHarness::GetTestingFactories();
    factories.push_back(TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory()});
    return factories;
  }

  void SetUp() override {
    ContextualSearchboxHandlerTestHarness::SetUp();

    web_ui_.set_web_contents(web_contents());
    mock_service_ = std::make_unique<MockOmniboxEverywhereService>(profile());

    handler_ = std::make_unique<OmniboxEverywhereHandlerPublic>(
        handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
        /*metrics_reporter=*/nullptr, &web_ui_, mock_service_.get(),
        base::BindRepeating(
            []() -> contextual_search::ContextualSearchSessionHandle* {
              return nullptr;
            }));
  }

  void TearDown() override {
    handler_.reset();
    mock_service_.reset();
    ContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::TestWebUI web_ui_;
  std::unique_ptr<MockOmniboxEverywhereService> mock_service_;
  testing::NiceMock<MockSearchboxPage> page_;
  mojo::Remote<searchbox::mojom::PageHandler> handler_remote_;
  std::unique_ptr<OmniboxEverywhereHandlerPublic> handler_;
};

TEST_F(OmniboxEverywhereHandlerTest,
       SubmitQueryVoiceSearchNavigatesToSearchUrlWithVoiceParam) {
  GURL captured_url;
  WindowOpenDisposition captured_disposition;
  ui::PageTransition captured_transition;

  EXPECT_CALL(*mock_service_,
              OpenUrl(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const GURL& url, WindowOpenDisposition disposition,
              ui::PageTransition transition,
              base::OnceCallback<void(content::NavigationHandle&)> callback) {
            captured_url = url;
            captured_disposition = disposition;
            captured_transition = transition;
          });

  handler_->SubmitQuery("weather today", /*mouse_button=*/0, /*alt_key=*/false,
                        /*ctrl_key=*/false, /*meta_key=*/false,
                        /*shift_key=*/false, /*is_voice_search=*/true);

  EXPECT_TRUE(captured_url.is_valid());
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB, captured_disposition);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(captured_transition,
                                           ui::PAGE_TRANSITION_GENERATED));

  std::string query_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(captured_url, "q", &query_value));
  EXPECT_EQ("weather today", query_value);

  std::string voice_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      captured_url, kVoiceSearchQueryParameterKey, &voice_param));
  EXPECT_EQ(kVoiceSearchQueryParameterValue, voice_param);

  std::string udm_param;
  EXPECT_FALSE(net::GetValueForKeyInQuery(captured_url, "udm", &udm_param));
}

TEST_F(OmniboxEverywhereHandlerTest,
       SubmitQueryVoiceSearchNonGoogleProviderDoesNotAttachGoogleParams) {
  // Set up a non-Google search provider.
  TemplateURLData data;
  data.SetShortName(u"example");
  data.SetKeyword(u"example");
  data.SetURL("https://example.com/search?q={searchTerms}");
  TemplateURL* non_google_provider =
      template_url_service()->Add(std::make_unique<TemplateURL>(data));
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      non_google_provider);

  GURL captured_url;
  WindowOpenDisposition captured_disposition;
  ui::PageTransition captured_transition;

  EXPECT_CALL(*mock_service_,
              OpenUrl(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const GURL& url, WindowOpenDisposition disposition,
              ui::PageTransition transition,
              base::OnceCallback<void(content::NavigationHandle&)> callback) {
            captured_url = url;
            captured_disposition = disposition;
            captured_transition = transition;
          });

  handler_->SubmitQuery("weather today", /*mouse_button=*/0, /*alt_key=*/false,
                        /*ctrl_key=*/false, /*meta_key=*/false,
                        /*shift_key=*/false, /*is_voice_search=*/true);

  EXPECT_TRUE(captured_url.is_valid());
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB, captured_disposition);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(captured_transition,
                                           ui::PAGE_TRANSITION_GENERATED));

  std::string query_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(captured_url, "q", &query_value));
  EXPECT_EQ("weather today", query_value);

  std::string voice_param;
  EXPECT_FALSE(net::GetValueForKeyInQuery(
      captured_url, kVoiceSearchQueryParameterKey, &voice_param));
}

TEST_F(OmniboxEverywhereHandlerTest, OpenProfilePickerCallsService) {
  EXPECT_CALL(*mock_service_, ShowProfilePicker()).Times(1);

  handler_->OpenProfilePicker();
}

TEST_F(OmniboxEverywhereHandlerTest, CleanupDrivePickerNotifiesService) {
  EXPECT_CALL(*mock_service_, OnDrivePickerClosed()).Times(1);

  handler_->CleanupDrivePicker();
}

TEST_F(OmniboxEverywhereHandlerTest, CreateAutocompleteMatchWithKeyword) {
  TemplateURLData data;
  data.SetShortName(u"example");
  data.SetKeyword(u"example");
  data.SetURL("https://example.com/search?q={searchTerms}");
  template_url_service()->Add(std::make_unique<TemplateURL>(data));

  AutocompleteMatch match;
  match.destination_url = GURL("https://example.com");
  match.associated_keyword = u"example";
  match.keyword = u"example";

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  auto mojom_match = handler_->CreateAutocompleteMatch(
      match, 0, bookmark_model, omnibox::GroupConfigMap(),
      template_url_service());

  ASSERT_TRUE(mojom_match.has_value());
  ASSERT_TRUE(mojom_match.value()->keyword_model);
  EXPECT_EQ(searchbox::mojom::KeywordType::kChip,
            mojom_match.value()->keyword_model->type);
  EXPECT_EQ("example", mojom_match.value()->keyword_model->keyword);
}

TEST_F(OmniboxEverywhereHandlerTest, ActivateKeywordDoesNotCrash) {
  handler_->ActivateKeyword(0, GURL("https://example.com"),
                            base::TimeTicks::Now(), /*is_mouse_event=*/true);
}

TEST_F(OmniboxEverywhereHandlerTest, OpenUrlForwardsToService) {
  GURL test_url("https://www.google.com");
  EXPECT_CALL(*mock_service_,
              OpenUrl(test_url, WindowOpenDisposition::CURRENT_TAB, testing::_,
                      testing::_))
      .WillOnce(
          [&](const GURL& url, WindowOpenDisposition disposition,
              ui::PageTransition transition,
              base::OnceCallback<void(content::NavigationHandle&)> callback) {
            EXPECT_TRUE(ui::PageTransitionCoreTypeIs(transition,
                                                     ui::PAGE_TRANSITION_LINK));
          });

  handler_->OpenUrl(test_url, WindowOpenDisposition::CURRENT_TAB,
                    base::NullCallback());
}

TEST_F(OmniboxEverywhereHandlerTest, DismissPromoUpdatesFrePreference) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreDismissed));

  handler_->DismissFre();

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereHandlerTest, FrePromoStateGatedByImpressionCount) {
  profile()->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kFreImpressionCount,
      omnibox_everywhere::prefs::kMaxFreImpressions);

  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetShowFre(false));

  mojo::Remote<searchbox::mojom::PageHandler> test_handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereHandler>(
      test_handler_remote.BindNewPipeAndPassReceiver(),
      mock_page.BindAndGetRemote(), /*metrics_reporter=*/nullptr, &web_ui_,
      mock_service_.get(),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));
  mock_page.FlushForTesting();
}

TEST_F(OmniboxEverywhereHandlerTest, SessionLifecycleDoesNotCrash) {
  handler_->NotifySessionStarted();
  handler_->NotifySessionAbandoned();
}

TEST_F(OmniboxEverywhereHandlerTest, FileContextHandoffDoesNotCrash) {
  const auto token = base::UnguessableToken::Create();
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.png";
  file_info->mime_type = "image/png";
  file_info->is_deletable = true;

  handler_->AddFileContextFromBrowser(token, std::move(file_info));
  handler_->OnContextUploadStatusChanged(
      token, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
}

TEST_F(OmniboxEverywhereHandlerTest,
       AddFileContextToPageInvokesAddFileContextFromBrowser) {
  const auto token = base::UnguessableToken::Create();
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "screenshot.png";
  file_info->mime_type = "image/png";
  file_info->is_deletable = true;

  // AddFileContextToPage (called by ContextualSearchboxScreenshareController)
  // should dynamically route through AddFileContextFromBrowser without
  // crashing.
  handler_->AddFileContextToPage(token, std::move(file_info));
}

}  // namespace
