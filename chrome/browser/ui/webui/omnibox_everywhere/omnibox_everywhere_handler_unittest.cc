// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/mojom/omnibox_everywhere.mojom.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_page_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/input_state.h"
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

class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  explicit TestingAimEligibilityService(Profile* profile,
                                        bool is_fusebox_eligible)
      : ChromeAimEligibilityService(*profile->GetPrefs(),
                                    /*template_url_service=*/nullptr,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*configuration=*/{}),
        is_fusebox_eligible_(is_fusebox_eligible) {}

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

  bool IsAimEligible() const override { return is_fusebox_eligible_; }
  bool IsFuseboxEligible() const override { return is_fusebox_eligible_; }
  bool IsAimAllowedByDse() const override { return is_fusebox_eligible_; }

  base::CallbackListSubscription RegisterEligibilityChangedCallback(
      base::RepeatingClosure callback) override {
    return callback_list_.Add(std::move(callback));
  }

  void SetFuseboxEligible(bool eligible) {
    is_fusebox_eligible_ = eligible;
    callback_list_.Notify();
  }

 private:
  bool is_fusebox_eligible_;
  base::RepeatingClosureList callback_list_;
};

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
  MOCK_METHOD(void, OnHotkeyDropdownOpened, (), (override));
  MOCK_METHOD(void, OnHotkeyDropdownClosed, (), (override));
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

  TestingAimEligibilityService* SetUpAimEligibilityService(
      bool is_fusebox_eligible) {
    return static_cast<TestingAimEligibilityService*>(
        AimEligibilityServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](bool fusebox_eligible, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<TestingAimEligibilityService>(
                      static_cast<TestingProfile*>(context), fusebox_eligible);
                },
                is_fusebox_eligible)));
  }

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

    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        version_info::Channel::UNKNOWN, "en-US", template_url_service(),
        /*variations_client=*/nullptr,
        std::move(query_controller_config_params));
    auto metrics_recorder_ptr =
        std::make_unique<MockContextualSearchMetricsRecorder>();

    contextual_session_handle_ =
        ContextualSearchServiceFactory::GetForProfile(profile())
            ->CreateSessionForTesting(std::move(query_controller_ptr),
                                      std::move(metrics_recorder_ptr));
    contextual_session_handle_->CheckSearchContentSharingSettings(
        profile()->GetPrefs());

    web_ui_.set_web_contents(web_contents());
    mock_service_ = std::make_unique<MockOmniboxEverywhereService>(profile());

    handler_ = std::make_unique<OmniboxEverywhereHandlerPublic>(
        handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
        /*metrics_reporter=*/nullptr, &web_ui_, mock_service_.get(),
        base::BindLambdaForTesting(
            [&]() { return contextual_session_handle_.get(); }));
  }

  void TearDown() override {
    handler_.reset();
    contextual_session_handle_.reset();
    mock_service_.reset();
    ContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::TestWebUI web_ui_;
  std::unique_ptr<MockOmniboxEverywhereService> mock_service_;
  testing::NiceMock<MockSearchboxPage> page_;
  mojo::Remote<searchbox::mojom::PageHandler> handler_remote_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextual_session_handle_;
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
      omnibox_everywhere::prefs::kFreIntroDismissed));

  handler_->DismissFre(searchbox::mojom::FreStage::kIntroModal);

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreIntroDismissed));
}

TEST_F(OmniboxEverywhereHandlerTest,
       DismissShortcutSetupWithoutHotkeySkipsReminder) {
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kHotkeyEnabled, false);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutSetupDismissed));

  handler_->DismissFre(searchbox::mojom::FreStage::kShortcutSetupChin);

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutSetupDismissed));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutReminderDismissed));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereHandlerTest,
       DismissShortcutSetupWithHotkeyAdvancesToReminder) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutSetupDismissed));

  handler_->DismissFre(searchbox::mojom::FreStage::kShortcutSetupChin);

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutSetupDismissed));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutReminderDismissed));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereHandlerTest, FrePromoStateGatedByImpressionCount) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kOmniboxEverywhereFre);

  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetFreState(testing::_))
      .WillOnce([](searchbox::mojom::FreStatePtr state) {
        EXPECT_EQ(state->stage, searchbox::mojom::FreStage::kIntroModal);
      });

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

  EXPECT_CALL(mock_page, SetFreState(testing::_))
      .WillOnce([](searchbox::mojom::FreStatePtr state) {
        EXPECT_EQ(state->stage, searchbox::mojom::FreStage::kShortcutSetupChin);
      });
  profile()->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kFreIntroImpressionCount,
      omnibox_everywhere::prefs::kMaxFreIntroImpressions);
  mock_page.FlushForTesting();
}

TEST_F(OmniboxEverywhereHandlerTest, FreDisabledFeatureFlagEmitsNoneStage) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(omnibox::kOmniboxEverywhereFre);

  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetFreState(testing::_))
      .WillOnce([](searchbox::mojom::FreStatePtr state) {
        EXPECT_EQ(state->stage, searchbox::mojom::FreStage::kNone);
      });

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

TEST_F(OmniboxEverywhereHandlerTest, FileContextValidationErrorHandoff) {
  const auto token = base::UnguessableToken::Create();
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "oversized.png";
  file_info->mime_type = "image/png";
  file_info->is_deletable = true;

  handler_->AddFileContextFromBrowser(token, std::move(file_info));
  handler_->OnContextUploadStatusChanged(
      token, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kValidationFailed,
      contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileTooLargeError);
}

TEST_F(OmniboxEverywhereHandlerTest, ScreenshotMenuDisabledAtMaxFiles) {
  // Screenshot commands are disabled when handler is null.
  EXPECT_FALSE(OmniboxEverywhereUI::IsScreenshotCommandEnabled(nullptr));

  // Screenshot commands are enabled initially with 0 files.
  EXPECT_TRUE(OmniboxEverywhereUI::IsScreenshotCommandEnabled(handler_.get()));

  // Add default max inputs.
  for (size_t i = 0; i < omnibox::kDefaultMaxTotalInputs; ++i) {
    contextual_session_handle_->CreateContextToken();
  }

  // Screenshot commands are disabled once max files limit is reached.
  EXPECT_FALSE(OmniboxEverywhereUI::IsScreenshotCommandEnabled(handler_.get()));
}

TEST_F(OmniboxEverywhereHandlerTest,
       SetHotkeyUpdatesLocalStateAndDispatchesState) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetFreState(testing::_)).Times(testing::AtLeast(1));

  mojo::Remote<searchbox::mojom::PageHandler> test_handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereHandler>(
      test_handler_remote.BindNewPipeAndPassReceiver(),
      mock_page.BindAndGetRemote(), /*metrics_reporter=*/nullptr, &web_ui_,
      mock_service_.get(),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));

  local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, false);
  handler->SetHotkey("Space+Alt+Shift");
  EXPECT_EQ(local_state->GetString(
                omnibox_everywhere::prefs::kOmniboxEverywhereHotkey),
            "Space+Alt+Shift");
  EXPECT_TRUE(
      local_state->GetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kFreShortcutSetupDismissed));
  mock_page.FlushForTesting();
}

TEST_F(OmniboxEverywhereHandlerTest,
       ShowAiModePrefChangeUpdatesAimPopupEligibility) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);

  testing::NiceMock<MockSearchboxPage> mock_page;
  mojo::Remote<searchbox::mojom::PageHandler> test_handler_remote;

  auto handler = std::make_unique<OmniboxEverywhereHandler>(
      test_handler_remote.BindNewPipeAndPassReceiver(),
      mock_page.BindAndGetRemote(), /*metrics_reporter=*/nullptr, &web_ui_,
      mock_service_.get(),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));

  // Disabling pref should push UpdateAimPopupEligibility(false).
  EXPECT_CALL(mock_page, UpdateAimPopupEligibility(false)).Times(1);
  profile()->GetPrefs()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode, false);
  mock_page.FlushForTesting();

  // Re-enabling pref should push UpdateAimPopupEligibility(true).
  EXPECT_CALL(mock_page, UpdateAimPopupEligibility(true)).Times(1);
  profile()->GetPrefs()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode, true);
  mock_page.FlushForTesting();
}

TEST_F(OmniboxEverywhereHandlerTest,
       AimEligibilityChangeUpdatesAimPopupEligibility) {
  auto* aim_service = SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);

  testing::NiceMock<MockSearchboxPage> mock_page;
  mojo::Remote<searchbox::mojom::PageHandler> test_handler_remote;

  auto handler = std::make_unique<OmniboxEverywhereHandler>(
      test_handler_remote.BindNewPipeAndPassReceiver(),
      mock_page.BindAndGetRemote(), /*metrics_reporter=*/nullptr, &web_ui_,
      mock_service_.get(),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));

  // Becoming ineligible via AimEligibilityService callback should push
  // UpdateAimPopupEligibility(false).
  EXPECT_CALL(mock_page, UpdateAimPopupEligibility(false)).Times(1);
  aim_service->SetFuseboxEligible(false);
  mock_page.FlushForTesting();

  // Re-enabling eligibility should push UpdateAimPopupEligibility(true).
  EXPECT_CALL(mock_page, UpdateAimPopupEligibility(true)).Times(1);
  aim_service->SetFuseboxEligible(true);
  mock_page.FlushForTesting();
}

TEST_F(OmniboxEverywhereHandlerTest,
       SetIsComposeboxDoesNotCrashWithNullController) {
  mojo::Remote<omnibox_everywhere::mojom::PageHandler> page_handler_remote;
  OmniboxEverywherePageHandler page_handler(
      page_handler_remote.BindNewPipeAndPassReceiver(), mojo::NullRemote(),
      /*web_ui_controller=*/nullptr);

  page_handler.SetIsComposebox(true);
  page_handler.SetIsComposebox(false);
}

TEST_F(OmniboxEverywhereHandlerTest, CalculateContextMenuAnchorPoint_LTR) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  ASSERT_FALSE(base::i18n::IsRTL());

  gfx::Rect anchor_rect(10, 20, 30, 40);
  gfx::Rect container_bounds(100, 200, 800, 600);

  // In LTR, anchor point should be anchor_rect.bottom_left() + container
  // offset. anchor_rect.bottom_left() is (10, 60), container offset is (100,
  // 200) -> (110, 260).
  gfx::Point expected_point =
      anchor_rect.bottom_left() + container_bounds.OffsetFromOrigin();
  EXPECT_EQ(OmniboxEverywhereUI::CalculateContextMenuAnchorPoint(
                anchor_rect, container_bounds),
            expected_point);
  EXPECT_EQ(OmniboxEverywhereUI::CalculateContextMenuAnchorPoint(
                anchor_rect, container_bounds),
            gfx::Point(110, 260));
}

TEST_F(OmniboxEverywhereHandlerTest, CalculateContextMenuAnchorPoint_RTL) {
  base::test::ScopedRestoreICUDefaultLocale locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  gfx::Rect anchor_rect(10, 20, 30, 40);
  gfx::Rect container_bounds(100, 200, 800, 600);

  // In RTL, anchor point should be anchor_rect.bottom_right() + container
  // offset. anchor_rect.bottom_right() is (40, 60), container offset is (100,
  // 200) -> (140, 260).
  gfx::Point expected_point =
      anchor_rect.bottom_right() + container_bounds.OffsetFromOrigin();
  EXPECT_EQ(OmniboxEverywhereUI::CalculateContextMenuAnchorPoint(
                anchor_rect, container_bounds),
            expected_point);
  EXPECT_EQ(OmniboxEverywhereUI::CalculateContextMenuAnchorPoint(
                anchor_rect, container_bounds),
            gfx::Point(140, 260));
}
}  // namespace
