// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/test_omnibox_view.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_features.h"
#include "lens_searchbox_handler.h"
#include "realbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

// Exclude desktop-only headers for WebuiOmniboxHandler and
// OmniboxComposeboxHandler, which are dedicated to the desktop Omnibox Popup
// and not compiled on Android.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#endif

namespace {
class RealboxHandlerPublic : public RealboxHandler {
 public:
  using RealboxHandler::RealboxHandler;
  using SearchboxHandler::autocomplete_controller;
  using SearchboxHandler::autocomplete_controller_observation_;
  using SearchboxHandler::client;
  using SearchboxHandler::omnibox_controller;
  using SearchboxHandler::OpenMatch;
  using SearchboxHandler::SetAutocompleteControllerForTesting;
};

class LensSearchboxHandlerPublic : public LensSearchboxHandler {
 public:
  using LensSearchboxHandler::LensSearchboxHandler;
  using SearchboxHandler::autocomplete_controller_observation_;
  using SearchboxHandler::client;
  using SearchboxHandler::omnibox_controller;
  using SearchboxHandler::OpenMatch;
  using SearchboxHandler::SetAutocompleteControllerForTesting;
};
}  // namespace

class SearchboxHandlerTest : public ::testing::Test {
 public:
  SearchboxHandlerTest() = default;

  SearchboxHandlerTest(const SearchboxHandlerTest&) = delete;
  SearchboxHandlerTest& operator=(const SearchboxHandlerTest&) = delete;
  ~SearchboxHandlerTest() override = default;

  content::TestWebUIDataSource* source() { return source_.get(); }
  TestingProfile* profile() { return profile_.get(); }

 protected:
  testing::NiceMock<MockSearchboxPage> page_;
  raw_ptr<testing::NiceMock<MockAutocompleteController>>
      autocomplete_controller_;
  raw_ptr<testing::NiceMock<MockOmniboxEditModel>> omnibox_edit_model_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestWebUIDataSource> source_;
  std::unique_ptr<TestingProfile> profile_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  void SetUp() override {
    source_ = content::TestWebUIDataSource::Create("test-data-source");

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();

    ASSERT_EQ(
        variations::VariationsIdsProvider::ForceIdsResult::SUCCESS,
        variations::VariationsIdsProvider::GetInstance()
            ->ForceVariationIdsForTesting(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));
  }

  void TearDown() override {
    omnibox_edit_model_ = nullptr;
    autocomplete_controller_ = nullptr;
  }
};

TEST_F(SearchboxHandlerTest, GetWebUIDataSourceDictSetsDragAndDrop) {
  base::DictValue strings = SearchboxHandler::GetWebUIDataSourceDict(profile());
  EXPECT_FALSE(*strings.FindBool("composeboxContextDragAndDropEnabled"));

  base::DictValue strings_with_drag = SearchboxHandler::GetWebUIDataSourceDict(
      profile(), {.session_allows_drag_and_drop = true});
  EXPECT_TRUE(
      *strings_with_drag.FindBool("composeboxContextDragAndDropEnabled"));
}

TEST_F(SearchboxHandlerTest, GetWebUIDataSourceDictSetsVoiceWaiting) {
  base::DictValue strings = SearchboxHandler::GetWebUIDataSourceDict(profile());
  EXPECT_NE(nullptr, strings.Find("voiceWaiting"));
}

TEST_F(SearchboxHandlerTest, GetWebUIDataSourceDictSetsVirtualFocusFlags) {
  // 1. Default state.
  {
    base::DictValue strings =
        SearchboxHandler::GetWebUIDataSourceDict(profile());
    EXPECT_FALSE(*strings.FindBool("realboxVirtualFocusNavigation"));
    EXPECT_FALSE(*strings.FindBool("omniboxPopupVirtualFocusNavigation"));
    EXPECT_FALSE(*strings.FindBool("lensOverlayVirtualFocusNavigation"));
    EXPECT_TRUE(*strings.FindBool("omniboxEverywhereVirtualFocusNavigation"));
    EXPECT_FALSE(*strings.FindBool("webuiBrowserVirtualFocusNavigation"));
  }

  // 2. Flags enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kRealboxVirtualFocusNavigation,
         features::kOmniboxPopupVirtualFocusNavigation,
         features::kLensOverlayVirtualFocusNavigation,
         features::kOmniboxEverywhereVirtualFocusNavigation,
         features::kWebuiBrowserVirtualFocusNavigation},
        {});
    base::DictValue strings =
        SearchboxHandler::GetWebUIDataSourceDict(profile());
    EXPECT_TRUE(*strings.FindBool("realboxVirtualFocusNavigation"));
    EXPECT_TRUE(*strings.FindBool("omniboxPopupVirtualFocusNavigation"));
    EXPECT_TRUE(*strings.FindBool("lensOverlayVirtualFocusNavigation"));
    EXPECT_TRUE(*strings.FindBool("omniboxEverywhereVirtualFocusNavigation"));
    EXPECT_TRUE(*strings.FindBool("webuiBrowserVirtualFocusNavigation"));
  }
}

TEST_F(SearchboxHandlerTest, GetWebUIDataSourceDictKeywordSpaceTriggering) {
  profile()->GetPrefs()->SetBoolean(omnibox::kKeywordSpaceTriggeringEnabled,
                                    false);
  base::DictValue strings = SearchboxHandler::GetWebUIDataSourceDict(profile());
  EXPECT_FALSE(*strings.FindBool("keywordSpaceTriggeringEnabled"));

  profile()->GetPrefs()->SetBoolean(omnibox::kKeywordSpaceTriggeringEnabled,
                                    true);
  strings = SearchboxHandler::GetWebUIDataSourceDict(profile());
  EXPECT_TRUE(*strings.FindBool("keywordSpaceTriggeringEnabled"));
}

TEST_F(SearchboxHandlerTest, KeywordSpaceTriggeringDynamicPrefChange) {
  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface;
  ui::UnownedUserDataHost unowned_user_data_host;
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowFeatures browser_window_features;
  SetupMockBrowserWindowInterface(browser_window_interface, profile(),
                                  browser_window_features,
                                  unowned_user_data_host);
#else
  ON_CALL(browser_window_interface, GetProfile())
      .WillByDefault(testing::Return(profile()));
  ON_CALL(browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
#endif
  webui::SetBrowserWindowInterface(web_contents.get(),
                                   &browser_window_interface);

  EXPECT_CALL(page_, SetKeywordSpaceTriggeringEnabled(true));
  auto handler = std::make_unique<RealboxHandlerPublic>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      page_.BindAndGetRemote(), profile(), web_contents.get(),
      base::BindLambdaForTesting(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));
  page_.FlushForTesting();

  EXPECT_CALL(page_, SetKeywordSpaceTriggeringEnabled(false));
  profile()->GetPrefs()->SetBoolean(omnibox::kKeywordSpaceTriggeringEnabled,
                                    false);
  page_.FlushForTesting();

  EXPECT_CALL(page_, SetKeywordSpaceTriggeringEnabled(true));
  profile()->GetPrefs()->SetBoolean(omnibox::kKeywordSpaceTriggeringEnabled,
                                    true);
  page_.FlushForTesting();
}

TEST_F(SearchboxHandlerTest, GetWebUIDataSourceDictLensSearchHint) {
  // 1. Default state: flag disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        omnibox::kWebUIOmniboxAskGAboutThisPage);
    base::DictValue strings =
        SearchboxHandler::GetWebUIDataSourceDict(profile());
    EXPECT_EQ(base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL)),
              *strings.FindString("lensSearchHint"));
  }

  // 2. Flag enabled, but param disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kWebUIOmniboxAskGAboutThisPage,
        {{"Omnibox_AskGLensSearchHintText", "false"}});
    base::DictValue strings =
        SearchboxHandler::GetWebUIDataSourceDict(profile());
    EXPECT_EQ(base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL)),
              *strings.FindString("lensSearchHint"));
  }

  // 3. Flag enabled AND param enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kWebUIOmniboxAskGAboutThisPage,
        {{"Omnibox_AskGLensSearchHintText", "true"}});
    base::DictValue strings =
        SearchboxHandler::GetWebUIDataSourceDict(profile());
    EXPECT_EQ(base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                  IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_TITLE)),
              *strings.FindString("lensSearchHint"));
  }
}

TEST_F(SearchboxHandlerTest, QuestionMarkKeywordInput) {
  content::RenderViewHostTestEnabler test_render_host_factories;
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface;
  ui::UnownedUserDataHost unowned_user_data_host;
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowFeatures browser_window_features;
  SetupMockBrowserWindowInterface(browser_window_interface, profile(),
                                  browser_window_features,
                                  unowned_user_data_host);
#else
  ON_CALL(browser_window_interface, GetProfile())
      .WillByDefault(testing::Return(profile()));
  ON_CALL(browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
#endif
  webui::SetBrowserWindowInterface(web_contents.get(),
                                   &browser_window_interface);

  auto handler = std::make_unique<RealboxHandlerPublic>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      page_.BindAndGetRemote(), profile(), web_contents.get(),
      base::BindLambdaForTesting(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));

  // Stop observing the AutocompleteController instance which will be destroyed.
  handler->autocomplete_controller_observation_.Reset();
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  auto* mock_autocomplete_controller = autocomplete_controller.get();
  handler->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler->omnibox_controller());
  auto* mock_omnibox_edit_model = omnibox_edit_model.get();
  handler->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(template_url_service);
  template_url_service->Load();
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  std::u16string input_text;
  EXPECT_CALL(*mock_omnibox_edit_model, SetUserText(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&input_text));

  AutocompleteInput input;
  EXPECT_CALL(*mock_autocomplete_controller, Start(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&input));

  handler->QueryAutocomplete(
      0, /*tab_id=*/std::nullopt, u"", /*prevent_inline_autocomplete=*/false, 0,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"?",
      searchbox::mojom::InputMethod::kKeyboard);

  EXPECT_TRUE(input.in_keyword_mode());
  EXPECT_TRUE(input.allow_exact_keyword_match());

  testing::Mock::VerifyAndClearExpectations(mock_omnibox_edit_model);
  testing::Mock::VerifyAndClearExpectations(mock_autocomplete_controller);

  handler.reset();
}

TEST_F(SearchboxHandlerTest, QueryAutocomplete_IsOnFocusDoesNotSetUserText) {
  content::RenderViewHostTestEnabler test_render_host_factories;
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface;
  ui::UnownedUserDataHost unowned_user_data_host;
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowFeatures browser_window_features;
  SetupMockBrowserWindowInterface(browser_window_interface, profile(),
                                  browser_window_features,
                                  unowned_user_data_host);
#else
  ON_CALL(browser_window_interface, GetProfile())
      .WillByDefault(testing::Return(profile()));
  ON_CALL(browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(unowned_user_data_host));
#endif
  webui::SetBrowserWindowInterface(web_contents.get(),
                                   &browser_window_interface);

  auto handler = std::make_unique<RealboxHandlerPublic>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      page_.BindAndGetRemote(), profile(), web_contents.get(),
      base::BindLambdaForTesting(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }));

  handler->autocomplete_controller_observation_.Reset();
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  handler->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler->omnibox_controller());
  auto* mock_omnibox_edit_model = omnibox_edit_model.get();
  handler->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  // When `is_on_focus` is true, SetUserText should NOT be called.
  EXPECT_CALL(*mock_omnibox_edit_model, SetUserText(_)).Times(0);

  handler->QueryAutocomplete(
      0, /*tab_id=*/std::nullopt, u"", /*prevent_inline_autocomplete=*/false, 0,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/true, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  testing::Mock::VerifyAndClearExpectations(mock_omnibox_edit_model);

  // When `is_on_focus` is false (user input), SetUserText SHOULD be called.
  EXPECT_CALL(*mock_omnibox_edit_model, SetUserText(std::u16string(u"test")))
      .Times(1);

  handler->QueryAutocomplete(
      1, /*tab_id=*/std::nullopt, u"test",
      /*prevent_inline_autocomplete=*/false, 4,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  testing::Mock::VerifyAndClearExpectations(mock_omnibox_edit_model);

  handler.reset();
}

class RealboxHandlerTest : public SearchboxHandlerTest {
 public:
  RealboxHandlerTest() = default;

  RealboxHandlerTest(const RealboxHandlerTest&) = delete;
  RealboxHandlerTest& operator=(const RealboxHandlerTest&) = delete;
  ~RealboxHandlerTest() override = default;

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<RealboxHandlerPublic> handler_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowFeatures browser_window_features_;
#endif
  ui::UnownedUserDataHost unowned_user_data_host_;

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

#if !BUILDFLAG(IS_ANDROID)
    SetupMockBrowserWindowInterface(browser_window_interface_, profile(),
                                    browser_window_features_,
                                    unowned_user_data_host_);
    webui::SetBrowserWindowInterface(web_contents_.get(),
                                     &browser_window_interface_);
#else
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    webui::SetBrowserWindowInterface(web_contents_.get(),
                                     &browser_window_interface_);
#endif

    handler_ = std::make_unique<RealboxHandlerPublic>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(), profile(), web_contents_.get(),
        base::BindLambdaForTesting(
            []() -> contextual_search::ContextualSearchSessionHandle* {
              return nullptr;
            }));
  }

  void TearDown() override {
    webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
    SearchboxHandlerTest::TearDown();
    handler_.reset();
  }
};

TEST_F(RealboxHandlerTest, RealboxLensVariationsContainsVariations) {
  base::DictValue strings = SearchboxHandler::GetWebUIDataSourceDict(profile());

  EXPECT_EQ("CGQ", *strings.FindString("searchboxLensVariations"));
}

namespace {
class MockSearchboxHandlerDelegate : public SearchboxHandler::Delegate {
 public:
  MOCK_METHOD(void,
              OnEmbeddedPermissionDialogChanged,
              (bool is_showing, const gfx::Size& prompt_size),
              (override));
  MOCK_METHOD(OmniboxController*, GetOmniboxController, (), (override));
};
}  // namespace

TEST_F(RealboxHandlerTest, OnPermissionPromptChanged) {
  MockSearchboxHandlerDelegate delegate;
  handler_->set_delegate(&delegate);

  // Case: is_showing=true, non-zero size (adds buffer)
  {
    EXPECT_CALL(page_,
                OnPermissionPromptChanged(true, gfx::Size(100 + 40, 200 + 40)));
    EXPECT_CALL(delegate, OnEmbeddedPermissionDialogChanged(
                              true, gfx::Size(100 + 40, 200 + 40)));
    handler_->OnPermissionPromptChanged(true, gfx::Size(100, 200));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=true, zero size (does not add buffer)
  {
    EXPECT_CALL(page_, OnPermissionPromptChanged(true, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(true, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(true, gfx::Size(0, 0));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=false, non-zero size (does not add buffer, size is 0, 0)
  {
    EXPECT_CALL(page_, OnPermissionPromptChanged(false, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(false, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(false, gfx::Size(100, 200));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=false, zero size (does not add buffer, size is 0, 0)
  {
    EXPECT_CALL(page_, OnPermissionPromptChanged(false, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(false, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(false, gfx::Size(0, 0));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }
}

TEST_F(RealboxHandlerTest, GetDriveDisclaimerStatus) {
  base::test::TestFuture<searchbox::mojom::DriveDisclaimerStatus> future;
  handler_->GetDriveDisclaimerStatus(future.GetCallback());
  EXPECT_EQ(searchbox::mojom::DriveDisclaimerStatus::kRestricted,
            future.Take());
}

TEST_F(RealboxHandlerTest, OnDriveUploadClicked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kComposeboxDriveContextMenuOption);

  base::test::TestFuture<searchbox::mojom::DriveUploadResponsePtr> future;
  handler_->OnDriveUploadClicked(future.GetCallback());
  auto response = future.Take();
  EXPECT_TRUE(response);
}

TEST_F(RealboxHandlerTest, AutocompleteController_Start) {
  // Stop observing the AutocompleteController instance which will be destroyed.
  handler_->autocomplete_controller_observation_.Reset();
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  autocomplete_controller_ = autocomplete_controller.get();
  handler_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));
  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler_->omnibox_controller());
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    SCOPED_TRACE("Empty input");

    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_)).Times(0);

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"", /*prevent_inline_autocomplete=*/false,
        0, omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
        /*is_on_focus=*/true, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input.text(), u"");
    EXPECT_EQ(input.focus_type(), metrics::OmniboxFocusType::INTERACTION_FOCUS);
    EXPECT_EQ(input.current_url().spec(), "");
    EXPECT_EQ(input.current_page_classification(),
              metrics::OmniboxEventProto::NTP_REALBOX);
    EXPECT_FALSE(input.lens_overlay_suggest_inputs().has_value());

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
  }
  {
    SCOPED_TRACE("Non-empty input");

    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"a", /*prevent_inline_autocomplete=*/false,
        0, omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
        /*is_on_focus=*/false, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input_text, u"a");
    EXPECT_EQ(input.text(), u"a");
    EXPECT_EQ(input.focus_type(),
              metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    EXPECT_EQ(input.current_url().spec(), "");
    EXPECT_EQ(input.current_page_classification(),
              metrics::OmniboxEventProto::NTP_REALBOX);
    EXPECT_FALSE(input.lens_overlay_suggest_inputs().has_value());

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
  }
}

TEST_F(RealboxHandlerTest, AutocompleteController_StartWithSuggestInventory) {
  // Stop observing the AutocompleteController instance which will be destroyed.
  handler_->autocomplete_controller_observation_.Reset();
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  autocomplete_controller_ = autocomplete_controller.get();
  handler_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));
  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler_->omnibox_controller());
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"a", /*prevent_inline_autocomplete=*/false,
        0, omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL,
        /*is_on_focus=*/false, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input_text, u"a");
    EXPECT_EQ(input.text(), u"a");
    EXPECT_EQ(input.focus_type(),
              metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    EXPECT_EQ(input.current_url().spec(), "");
    EXPECT_EQ(input.current_page_classification(),
              metrics::OmniboxEventProto::NTP_REALBOX);
    EXPECT_EQ(input.suggest_inventory(),
              omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL);

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
  }
}

TEST_F(RealboxHandlerTest, InputMethodTest) {
  // Stop observing the `AutocompleteController` instance which will be
  // destroyed.
  handler_->autocomplete_controller_observation_.Reset();
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  autocomplete_controller_ = autocomplete_controller.get();
  handler_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));
  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler_->omnibox_controller());
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    SCOPED_TRACE("With smart compose input method");
    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"test query",
        /*prevent_inline_autocomplete=*/false, 10,
        omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL,
        /*is_on_focus=*/false, /*keyword=*/"",
        searchbox::mojom::InputMethod::kSmartCompose);

    EXPECT_EQ(input_text, u"test query");
    EXPECT_EQ(input.text(), u"test query");
    EXPECT_EQ(input.input_method(),
              omnibox::metrics::ChromeSearchboxStats::SMART_COMPOSE);

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
  }

  {
    SCOPED_TRACE("Default input method");
    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"another query",
        /*prevent_inline_autocomplete=*/false, 13,
        omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL,
        /*is_on_focus=*/false, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input_text, u"another query");
    EXPECT_EQ(input.text(), u"another query");
    EXPECT_EQ(input.input_method(),
              omnibox::metrics::ChromeSearchboxStats::KEYBOARD);

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
  }
}

TEST_F(RealboxHandlerTest,
       GetCyclingPlaceholderConfig_NoAimEligibilityServiceReturnsEmpty) {
  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetCyclingPlaceholderConfig(future.GetCallback());
  auto config = future.Take();

  ASSERT_EQ(config->texts.size(), 0u);
  ASSERT_EQ(config->change_text_animation_interval.InMilliseconds(), 2000u);
  ASSERT_EQ(config->fade_text_animation_duration.InMilliseconds(), 250u);
}

namespace {
std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  auto service = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr,
      /*identity_manager=*/nullptr, AimEligibilityService::Configuration{});
  return service;
}
}  // namespace

class SearchboxHandlerAimEligibilityTest : public RealboxHandlerTest {
 public:
  SearchboxHandlerAimEligibilityTest() = default;
  ~SearchboxHandlerAimEligibilityTest() override = default;

 protected:
  raw_ptr<testing::NiceMock<MockAimEligibilityService>>
      mock_aim_eligibility_service_ = nullptr;

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockAimEligibilityService));

    mock_aim_eligibility_service_ =
        static_cast<testing::NiceMock<MockAimEligibilityService>*>(
            AimEligibilityServiceFactory::GetForProfile(profile()));

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    handler_ = std::make_unique<RealboxHandlerPublic>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(), profile(), web_contents_.get(),
        base::BindLambdaForTesting(
            []() -> contextual_search::ContextualSearchSessionHandle* {
              return nullptr;
            }));
  }

  void TearDown() override {
    mock_aim_eligibility_service_ = nullptr;
    RealboxHandlerTest::TearDown();
  }
};

TEST_F(SearchboxHandlerAimEligibilityTest,
       GetCyclingPlaceholderConfig_AimEligibleReturnsEvergreenPlaceholders) {
  ON_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillByDefault(testing::Return(true));

  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetCyclingPlaceholderConfig(future.GetCallback());
  auto result = future.Take();

  ASSERT_EQ(result->texts.size(), 4u);
  EXPECT_EQ(result->texts[0], u"Ask Google");
  EXPECT_EQ(result->texts[1], u"Research a topic");
  EXPECT_EQ(result->texts[2], u"Learn a new skill");
  EXPECT_EQ(result->texts[3], u"Get advice");
}

TEST_F(SearchboxHandlerAimEligibilityTest,
       GetCyclingPlaceholderConfig_NotAimEligibleReturnsEmpty) {
  // Explicit: the mock's constructor defaults IsAimEligible() to true.
  ON_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillByDefault(testing::Return(false));

  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetCyclingPlaceholderConfig(future.GetCallback());
  auto result = future.Take();

  ASSERT_EQ(result->texts.size(), 0u);
}

TEST_F(RealboxHandlerTest, AddFileContext) {
  const auto token = base::UnguessableToken::Create();
  const std::string image_data_url = "data:image/png;base64,sometestdata";
  const bool is_deletable = true;

  // SelectedFileInfoPtr is a move-only type, so capture it in the lambda.
  searchbox::mojom::SelectedFileInfoPtr captured_file_info;
  EXPECT_CALL(page_, AddFileContext(token, testing::_))
      .Times(1)
      .WillOnce([&](const base::UnguessableToken&,
                    searchbox::mojom::SelectedFileInfoPtr info) {
        captured_file_info = std::move(info);
      });

  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "Visual Selection";
  file_info->mime_type = "image/png";
  file_info->image_data_url = image_data_url;
  file_info->is_deletable = is_deletable;
  handler_->SearchboxHandler::AddFileContextFromBrowser(token,
                                                        file_info.Clone());
  page_.FlushForTesting();

  ASSERT_TRUE(captured_file_info);
  ASSERT_EQ(captured_file_info->file_name, file_info->file_name);
  ASSERT_EQ(captured_file_info->mime_type, file_info->mime_type);
  ASSERT_EQ(captured_file_info->image_data_url, file_info->image_data_url);
  ASSERT_EQ(captured_file_info->is_deletable, file_info->is_deletable);
}

TEST_F(RealboxHandlerTest, ForceShowDescriptionNeverEnabledForRealbox) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGShowFirstDescription", "true"}});

  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);

  AutocompleteMatch match(provider.get(), 1000, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.description = u"Description 1";

  auto fake_autocomplete_controller =
      std::make_unique<FakeAutocompleteController>(&task_environment_);
  fake_autocomplete_controller->providers_.push_back(provider);
  fake_autocomplete_controller->published_result_.AppendMatches({match});

  handler_->autocomplete_controller_observation_.Reset();
  handler_->SetAutocompleteControllerForTesting(
      std::move(fake_autocomplete_controller));

  searchbox::mojom::AutocompleteResultPtr received_result;
  EXPECT_CALL(page_, AutocompleteResultChanged)
      .WillOnce(
          [&received_result](searchbox::mojom::AutocompleteResultPtr result) {
            received_result = std::move(result);
          });

  handler_->OnResultChanged(handler_->autocomplete_controller(), false);
  page_.FlushForTesting();

  ASSERT_TRUE(received_result);
  ASSERT_EQ(1u, received_result->matches.size());
  EXPECT_FALSE(received_result->matches[0]->show_contextual_description);
}

class LensSearchboxHandlerTest : public SearchboxHandlerTest {
 public:
  LensSearchboxHandlerTest() = default;

  LensSearchboxHandlerTest(const LensSearchboxHandlerTest&) = delete;
  LensSearchboxHandlerTest& operator=(const LensSearchboxHandlerTest&) = delete;
  ~LensSearchboxHandlerTest() override = default;

 protected:
  std::unique_ptr<testing::NiceMock<MockLensSearchboxClient>>
      lens_searchbox_client_;
  std::unique_ptr<LensSearchboxHandlerPublic> handler_;

 private:
  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    // Set a mock LensSearchboxClient.
    lens_searchbox_client_ =
        std::make_unique<testing::NiceMock<MockLensSearchboxClient>>();

    handler_ = std::make_unique<LensSearchboxHandlerPublic>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(), profile(),
        /*web_contents=*/nullptr, lens_searchbox_client_.get());
  }
};

TEST_F(LensSearchboxHandlerTest, Lens_AutocompleteController_Start) {
  // Stop observing the AutocompleteController instance which will be destroyed.
  handler_->autocomplete_controller_observation_.Reset();
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  autocomplete_controller_ = autocomplete_controller.get();
  handler_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));
  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler_->omnibox_controller());
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    SCOPED_TRACE("Empty input");

    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_)).Times(0);

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    EXPECT_CALL(*lens_searchbox_client_, GetPageClassification())
        .Times(1)
        .WillOnce(Return(metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX));

    GURL page_url("https://example.com");
    EXPECT_CALL(*lens_searchbox_client_, GetPageURL())
        .Times(1)
        .WillOnce(ReturnRef(page_url));

    lens::proto::LensOverlaySuggestInputs suggest_inputs;
    suggest_inputs.set_encoded_image_signals("xyz");
    suggest_inputs.set_encoded_request_id("abc");
    suggest_inputs.set_search_session_id("123");
    suggest_inputs.set_encoded_visual_search_interaction_log_data("321");
    EXPECT_CALL(*lens_searchbox_client_, GetLensSuggestInputs())
        .WillRepeatedly(Return(suggest_inputs));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"", /*prevent_inline_autocomplete=*/false,
        0, omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
        /*is_on_focus=*/true, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input.text(), u"");
    EXPECT_EQ(input.focus_type(), metrics::OmniboxFocusType::INTERACTION_FOCUS);
    EXPECT_EQ(input.current_url(), page_url);
    EXPECT_EQ(input.current_page_classification(),
              metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->encoded_image_signals(),
              suggest_inputs.encoded_image_signals());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->encoded_request_id(),
              suggest_inputs.encoded_request_id());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->search_session_id(),
              suggest_inputs.search_session_id());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()
                  ->encoded_visual_search_interaction_log_data(),
              suggest_inputs.encoded_visual_search_interaction_log_data());

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
    testing::Mock::VerifyAndClearExpectations(lens_searchbox_client_.get());
  }
  {
    SCOPED_TRACE("Non-empty input");

    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    EXPECT_CALL(*lens_searchbox_client_, GetPageClassification())
        .Times(1)
        .WillOnce(Return(metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX));

    GURL page_url("https://example.com");
    EXPECT_CALL(*lens_searchbox_client_, GetPageURL())
        .Times(1)
        .WillOnce(ReturnRef(page_url));

    lens::proto::LensOverlaySuggestInputs suggest_inputs;
    suggest_inputs.set_encoded_image_signals("xyz");
    suggest_inputs.set_encoded_request_id("abc");
    suggest_inputs.set_search_session_id("123");
    suggest_inputs.set_encoded_visual_search_interaction_log_data("321");
    EXPECT_CALL(*lens_searchbox_client_, GetLensSuggestInputs())
        .WillRepeatedly(Return(suggest_inputs));

    handler_->QueryAutocomplete(
        0, /*tab_id=*/std::nullopt, u"a",
        /*prevent_inline_autocomplete=*/false, 0,
        omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
        /*is_on_focus=*/false, /*keyword=*/"",
        searchbox::mojom::InputMethod::kKeyboard);

    EXPECT_EQ(input_text, u"a");
    EXPECT_EQ(input.text(), u"a");
    EXPECT_EQ(input.focus_type(),
              metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    EXPECT_EQ(input.current_url(), page_url);
    EXPECT_EQ(input.current_page_classification(),
              metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->encoded_image_signals(),
              suggest_inputs.encoded_image_signals());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->encoded_request_id(),
              suggest_inputs.encoded_request_id());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()->search_session_id(),
              suggest_inputs.search_session_id());
    EXPECT_EQ(input.lens_overlay_suggest_inputs()
                  ->encoded_visual_search_interaction_log_data(),
              suggest_inputs.encoded_visual_search_interaction_log_data());

    testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
    testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
    testing::Mock::VerifyAndClearExpectations(lens_searchbox_client_.get());
  }
  {
    SCOPED_TRACE("Icon override");

    const char search_icon[] =
        "//resources/cr_components/searchbox/icons/search_spark.svg";
    const std::string& svg_name = handler_->AutocompleteIconToResourceName(
        features::IsRoundedIconsEnabled()
            ? omnibox::kSubdirectoryArrowRightIcon
            : omnibox::kSubdirectoryArrowRightOldIcon);

    EXPECT_EQ(svg_name, search_icon);
  }
}

// WebuiOmniboxHandler is dedicated to the desktop Omnibox Popup and out of
// scope for Android WebUI NTP.
#if !BUILDFLAG(IS_ANDROID)
namespace {
class FakeOmniboxPopupView : public OmniboxPopupView {
 public:
  using OmniboxPopupView::OmniboxPopupView;
  bool IsOpen() const override { return false; }
  void InvalidateLine(size_t line) override {}
  void UpdatePopupAppearance() override {}
  void ProvideButtonFocusHint(size_t line) override {}
  void OnDragCanceled() override {}
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override {}
  bool IsSelectionPopupControlled() const override { return false; }
};
}  // namespace

class WebuiOmniboxHandlerPublic : public WebuiOmniboxHandler {
 public:
  using SearchboxHandler::autocomplete_controller;
  using SearchboxHandler::autocomplete_controller_observation_;
  using SearchboxHandler::client;
  using SearchboxHandler::CreateAutocompleteMatch;
  using SearchboxHandler::omnibox_controller;
  using SearchboxHandler::OpenMatch;
  using SearchboxHandler::SetAutocompleteControllerForTesting;
  using WebuiOmniboxHandler::WebuiOmniboxHandler;
};

class WebuiOmniboxHandlerTest : public SearchboxHandlerTest {
 public:
  WebuiOmniboxHandlerTest() = default;

  WebuiOmniboxHandlerTest(const WebuiOmniboxHandlerTest&) = delete;
  WebuiOmniboxHandlerTest& operator=(const WebuiOmniboxHandlerTest&) = delete;
  ~WebuiOmniboxHandlerTest() override = default;

 protected:
  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    omnibox_controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_ui_.set_web_contents(web_contents_.get());

    popup_view_ =
        std::make_unique<FakeOmniboxPopupView>(omnibox_controller_.get());
    omnibox_controller_->edit_model()->set_popup_view(popup_view_.get());

    test_omnibox_view_ =
        std::make_unique<TestOmniboxView>(omnibox_controller_.get());

    EXPECT_CALL(page_, AutocompleteResultChanged(testing::_)).Times(1);

    handler_ = std::make_unique<WebuiOmniboxHandlerPublic>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(),
        /*metrics_reporter=*/nullptr, omnibox_controller_.get(), &web_ui_,
        base::BindLambdaForTesting(
            []() -> contextual_search::ContextualSearchSessionHandle* {
              return nullptr;
            }));
  }

  void TearDown() override {
    handler_.reset();
    omnibox_controller_->edit_model()->set_popup_view(nullptr);
    popup_view_.reset();
    SearchboxHandlerTest::TearDown();
  }

  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<OmniboxPopupUI> omnibox_popup_ui_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<FakeOmniboxPopupView> popup_view_;
  std::unique_ptr<TestOmniboxView> test_omnibox_view_;
  std::unique_ptr<WebuiOmniboxHandlerPublic> handler_;
};

TEST_F(WebuiOmniboxHandlerTest, WebuiOmniboxUpdatesSelection) {
  searchbox::mojom::OmniboxPopupSelectionPtr old_selection;
  searchbox::mojom::OmniboxPopupSelectionPtr selection;
  EXPECT_CALL(page_, UpdateSelection)
      .Times(4)
      .WillRepeatedly([&old_selection, &selection](
                          searchbox::mojom::OmniboxPopupSelectionPtr arg0,
                          searchbox::mojom::OmniboxPopupSelectionPtr arg1) {
        old_selection = std::move(arg0);
        selection = std::move(arg1);
      });

  handler_->OnSelectionChanged(
      OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch),
      OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL));
  page_.FlushForTesting();
  EXPECT_EQ(0, selection->line);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kNormal, selection->state);

  handler_->OnSelectionChanged(
      OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL),
      OmniboxPopupSelection(1, OmniboxPopupSelection::KEYWORD_MODE));
  page_.FlushForTesting();
  EXPECT_EQ(1, selection->line);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kKeywordMode,
            selection->state);

  handler_->OnSelectionChanged(
      OmniboxPopupSelection(2, OmniboxPopupSelection::NORMAL),
      OmniboxPopupSelection(2, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
                            4));
  page_.FlushForTesting();
  EXPECT_EQ(2, selection->line);
  EXPECT_EQ(4, selection->action_index);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kFocusedButtonAction,
            selection->state);

  handler_->OnSelectionChanged(
      OmniboxPopupSelection(3, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION, 4),
      OmniboxPopupSelection(
          3, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION));
  page_.FlushForTesting();
  EXPECT_EQ(3, selection->line);
  EXPECT_EQ(
      searchbox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion,
      selection->state);
}

TEST_F(WebuiOmniboxHandlerTest, OnActiveTabChanged_SavesAndRestoresState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kWebUIOmniboxFullPopup);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  auto web_contents1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ON_CALL(tab1, GetContents())
      .WillByDefault(testing::Return(web_contents1.get()));
  ON_CALL(tab2, GetContents())
      .WillByDefault(testing::Return(web_contents2.get()));

  // Set up initial state in model.
  omnibox_controller_->edit_model()->SetUserText(u"test1");
  // Call OnActiveTabChanged to set active tab to tab1.
  MockTabListInterface tab_list;
  handler_->OnActiveTabChanged(tab_list, &tab1);
  // Now change text in model and view to simulate user input in tab1.
  omnibox_controller_->edit_model()->SetUserText(u"test1_modified");
  test_omnibox_view_->SetWindowTextAndCaretPos(u"test1_modified", 0, false,
                                               false);
  // Call OnActiveTabChanged to switch to tab2. This should save state for tab1.
  handler_->OnActiveTabChanged(tab_list, &tab2);
  // Now switch back to tab1. This should restore state.
  handler_->OnActiveTabChanged(tab_list, &tab1);

  // Verify model text was restored.
  EXPECT_EQ(u"test1_modified", omnibox_controller_->edit_model()->user_text());
}

TEST_F(WebuiOmniboxHandlerTest,
       CreateAutocompleteMatch_ContextualSearchIconOverride) {
  AutocompleteMatch match;
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.destination_url = GURL("https://example.com");

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmark_model->LoadEmptyForTest();

  auto mojom_match = handler_->CreateAutocompleteMatch(
      match, 0, bookmark_model, omnibox::GroupConfigMap(),
      omnibox_controller_->client()->GetTemplateURLService());

  ASSERT_TRUE(mojom_match.has_value());
  EXPECT_EQ(mojom_match.value()->icon_path,
            searchbox_internal::kReplyRotated180IconResourceName);
}

TEST_F(
    WebuiOmniboxHandlerTest,
    CreateAutocompleteMatch_ContextualSearchIconOverride_AskGSwapSuggestionIconEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGSwapSuggestionIcon", "true"}});

  AutocompleteMatch match;
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.destination_url = GURL("https://example.com");

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmark_model->LoadEmptyForTest();

  auto mojom_match = handler_->CreateAutocompleteMatch(
      match, 0, bookmark_model, omnibox::GroupConfigMap(),
      omnibox_controller_->client()->GetTemplateURLService());

  ASSERT_TRUE(mojom_match.has_value());
  EXPECT_EQ(mojom_match.value()->icon_path,
            searchbox_internal::kSearchSparkIconResourceName);
}

TEST_F(
    WebuiOmniboxHandlerTest,
    CreateAutocompleteMatch_ContextualSearchIconOverride_AskGSwapIconEnabledOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGSwapIcon", "true"}});

  AutocompleteMatch match;
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match.destination_url = GURL("https://example.com");

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmark_model->LoadEmptyForTest();

  auto mojom_match = handler_->CreateAutocompleteMatch(
      match, 0, bookmark_model, omnibox::GroupConfigMap(),
      omnibox_controller_->client()->GetTemplateURLService());

  ASSERT_TRUE(mojom_match.has_value());
  EXPECT_EQ(mojom_match.value()->icon_path,
            searchbox_internal::kReplyRotated180IconResourceName);
}

TEST_F(WebuiOmniboxHandlerTest,
       CreateAutocompleteMatch_PopulatesFuseboxAction) {
  AutocompleteMatch match;
  match.destination_url = GURL("https://example.com");

  omnibox::SuggestTemplateInfo suggest_template;
  auto* fusebox_action = suggest_template.mutable_fusebox_action();
  fusebox_action->set_preselected_tool(omnibox::TOOL_MODE_DEEP_SEARCH);
  fusebox_action->set_preferred_inventory(
      omnibox::SUGGEST_INVENTORY_BRAINSTORM);
  fusebox_action->set_query_action_override(
      omnibox::SuggestTemplateInfo::FuseboxAction::QUERY_ACTION_PASTE);
  fusebox_action->set_searchbox_override(
      omnibox::SuggestTemplateInfo::FuseboxAction::
          SEARCHBOX_OVERRIDE_COMPOSEBOX);
  match.suggest_template = suggest_template;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmark_model->LoadEmptyForTest();

  auto mojom_match = handler_->CreateAutocompleteMatch(
      match, 0, bookmark_model, omnibox::GroupConfigMap(),
      omnibox_controller_->client()->GetTemplateURLService());

  ASSERT_TRUE(mojom_match.has_value());
  ASSERT_TRUE(mojom_match.value()->fusebox_action);
  EXPECT_EQ(mojom_match.value()->fusebox_action->preselected_tool,
            omnibox::TOOL_MODE_DEEP_SEARCH);
  EXPECT_EQ(mojom_match.value()->fusebox_action->preferred_inventory,
            omnibox::SUGGEST_INVENTORY_BRAINSTORM);
  EXPECT_EQ(mojom_match.value()->fusebox_action->query_action_override,
            fusebox_action::mojom::QueryActionOverride::kPaste);
  EXPECT_EQ(mojom_match.value()->fusebox_action->searchbox_override,
            fusebox_action::mojom::SearchboxOverride::kComposebox);
}

TEST_F(WebuiOmniboxHandlerTest, OpenAutocompleteMatch_KeyboardModifiers) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);
  AutocompleteMatch match(provider.get(), 1000, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  match.destination_url = GURL("https://example.com");

  auto fake_autocomplete_controller =
      std::make_unique<FakeAutocompleteController>(&task_environment_);
  fake_autocomplete_controller->providers_.push_back(provider);
  fake_autocomplete_controller->internal_result_.AppendMatches({match});
  fake_autocomplete_controller->published_result_.AppendMatches({match});
  handler_->autocomplete_controller_observation_.Reset();
  handler_->SetAutocompleteControllerForTesting(
      std::move(fake_autocomplete_controller));

  TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(omnibox_controller_->client());
  EXPECT_CALL(*client,
              OnAutocompleteAccept(_, _, WindowOpenDisposition::NEW_WINDOW, _,
                                   _, _, _, _, _, _, _))
      .Times(1);

  auto modifiers = searchbox::mojom::ActionModifiers::New();
  modifiers->shift_key = true;
  handler_->OpenAutocompleteMatch(0, GURL("https://example.com"),
                                  /*are_matches_showing=*/false,
                                  /*mouse_button=*/0, std::move(modifiers),
                                  /*via_keyboard=*/true);
}

TEST_F(WebuiOmniboxHandlerTest, OpenLensSearch) {
  // Set a mock AutocompleteController.
  auto mock_client = std::make_unique<MockAutocompleteProviderClient>();
  auto* mock_client_ptr = mock_client.get();
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::move(mock_client), 0);

  handler_->autocomplete_controller_observation_.Reset();
  handler_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  // Enable kAskGLensChipRoute to trigger OpenLensOverlay path.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGLensChipRoute", "true"}});

  EXPECT_CALL(*mock_client_ptr,
              OpenLensOverlay(
                  true, lens::LensOverlayInvocationSource::kOmniboxPopupButton))
      .Times(1);

  omnibox_controller_->edit_model()->SetUserText(u"query in progress");
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());

  handler_->OpenLensSearch();

  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());
}

TEST_F(WebuiOmniboxHandlerTest, OpenMatchResumesNavigationWhenNoDialogShown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineExplicitChoiceDialog);
  auto* client = static_cast<TestOmniboxClient*>(omnibox_controller_->client());
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.destination_url = GURL("https://www.example.com/?q=foo");
  match.keyword = u"example";
  EXPECT_CALL(*client, ShowConfirmationDialogIfDefaultSearchExtensionControlled(
                           match.destination_url, testing::_))
      .WillOnce([](const GURL&,
                   base::OnceCallback<void(
                       OmniboxClient::ExtensionControlledDialogResult)> cb) {
        // No dialog is shown; report that rather than a user decision.
        std::move(cb).Run(
            OmniboxClient::ExtensionControlledDialogResult::kNoDialogShown);
        return true;
      })
      .WillRepeatedly(testing::Return(false));

  // The withheld navigation must still happen.
  EXPECT_CALL(*client, OnAutocompleteAccept(match.destination_url, testing::_,
                                            testing::_, testing::_, testing::_,
                                            testing::_, testing::_, testing::_,
                                            testing::_, testing::_, testing::_))
      .Times(1);
  handler_->OpenMatch(OmniboxPopupSelection(0), match,
                      WindowOpenDisposition::CURRENT_TAB,
                      base::TimeTicks::Now());
}

TEST_F(WebuiOmniboxHandlerTest, OpenMatchDropsNavigationWhenDialogCancelled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kSearchEngineExplicitChoiceDialog);
  auto* client = static_cast<TestOmniboxClient*>(omnibox_controller_->client());
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.destination_url = GURL("https://www.example.com/?q=foo");
  match.keyword = u"example";
  EXPECT_CALL(*client, ShowConfirmationDialogIfDefaultSearchExtensionControlled(
                           match.destination_url, testing::_))
      .WillOnce([](const GURL&,
                   base::OnceCallback<void(
                       OmniboxClient::ExtensionControlledDialogResult)> cb) {
        std::move(cb).Run(
            OmniboxClient::ExtensionControlledDialogResult::kCancel);
        return true;
      });
  EXPECT_CALL(*client, OnAutocompleteAccept(testing::_, testing::_, testing::_,
                                            testing::_, testing::_, testing::_,
                                            testing::_, testing::_, testing::_,
                                            testing::_, testing::_))
      .Times(0);

  handler_->OpenMatch(OmniboxPopupSelection(0), match,
                      WindowOpenDisposition::CURRENT_TAB,
                      base::TimeTicks::Now());
}

TEST_F(WebuiOmniboxHandlerTest,
       ForceShowDescriptionForFirstContextualMatch_HeaderEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGShowFirstDescription", "true"}});

  page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&page_);

  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);

  AutocompleteMatch match1(provider.get(), 1000, false,
                           AutocompleteMatchType::SEARCH_SUGGEST);
  match1.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match1.description = u"Description 1";

  AutocompleteMatch match2(provider.get(), 900, false,
                           AutocompleteMatchType::SEARCH_SUGGEST);
  match2.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match2.description = u"Description 2";

  auto fake_autocomplete_controller =
      std::make_unique<FakeAutocompleteController>(&task_environment_);
  fake_autocomplete_controller->providers_.push_back(provider);
  fake_autocomplete_controller->published_result_.AppendMatches(
      {match1, match2});

  handler_->autocomplete_controller_observation_.Reset();
  handler_->SetAutocompleteControllerForTesting(
      std::move(fake_autocomplete_controller));

  searchbox::mojom::AutocompleteResultPtr received_result;
  EXPECT_CALL(page_, AutocompleteResultChanged)
      .WillOnce(
          [&received_result](searchbox::mojom::AutocompleteResultPtr result) {
            received_result = std::move(result);
          });

  handler_->OnResultChanged(handler_->autocomplete_controller(), false);
  page_.FlushForTesting();

  ASSERT_TRUE(received_result);
  ASSERT_EQ(2u, received_result->matches.size());
  EXPECT_TRUE(received_result->matches[0]
                  ->show_contextual_description);  // First match -> True
  EXPECT_FALSE(received_result->matches[1]
                   ->show_contextual_description);  // Second match -> False
}

TEST_F(WebuiOmniboxHandlerTest,
       ForceShowDescriptionForFirstContextualMatch_HeaderNotEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGShowFirstDescription", "true"}});

  page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&page_);

  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);

  AutocompleteMatch match1(provider.get(), 1000, false,
                           AutocompleteMatchType::SEARCH_SUGGEST);
  match1.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH;
  match1.description = u"Description 1";

  auto fake_autocomplete_controller =
      std::make_unique<FakeAutocompleteController>(&task_environment_);
  fake_autocomplete_controller->providers_.push_back(provider);

  // Populate header for the group to make it not empty
  omnibox::GroupConfigMap groups_map;
  groups_map[omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH].set_header_text(
      "Contextual Header");
  fake_autocomplete_controller->published_result_.MergeSuggestionGroupsMap(
      groups_map);
  fake_autocomplete_controller->published_result_.AppendMatches({match1});

  handler_->autocomplete_controller_observation_.Reset();
  handler_->SetAutocompleteControllerForTesting(
      std::move(fake_autocomplete_controller));

  searchbox::mojom::AutocompleteResultPtr received_result;
  EXPECT_CALL(page_, AutocompleteResultChanged)
      .WillOnce(
          [&received_result](searchbox::mojom::AutocompleteResultPtr result) {
            received_result = std::move(result);
          });

  handler_->OnResultChanged(handler_->autocomplete_controller(), false);
  page_.FlushForTesting();

  ASSERT_TRUE(received_result);
  ASSERT_EQ(1u, received_result->matches.size());
  EXPECT_FALSE(received_result->matches[0]
                   ->show_contextual_description);  // Header not empty -> False
}

class WebuiOmniboxHandlerTabScopingTest : public WebuiOmniboxHandlerTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(omnibox::kWebUIOmniboxFullPopup);
    WebuiOmniboxHandlerTest::SetUp();

    browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    SetupMockBrowserWindowInterface(
        *browser_window_interface_, profile(), browser_window_features_,
        unowned_user_data_host_, &mock_active_tab_, web_contents_.get());

    tab_list_ = std::make_unique<testing::NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);
    SetupMockTabListInterface(*tab_list_, &mock_active_tab_);
    SetupMockTabInterface(mock_active_tab_, web_contents_.get(), profile(),
                          browser_window_interface_.get(),
                          &active_tab_user_data_host_);

    auto mock_autocomplete_controller =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::make_unique<MockAutocompleteProviderClient>(), 0);
    mock_autocomplete_controller_ = mock_autocomplete_controller.get();
    handler_->autocomplete_controller_observation_.Reset();
    handler_->SetAutocompleteControllerForTesting(
        std::move(mock_autocomplete_controller));
  }

  void TearDown() override {
    mock_autocomplete_controller_ = nullptr;
    if (web_contents_) {
      webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
    }
    tab_list_registration_.reset();
    tab_list_.reset();
    browser_window_interface_.reset();
    WebuiOmniboxHandlerTest::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateBackgroundTab(
      tabs::MockTabInterface& mock_tab) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    SetupMockTabInterface(mock_tab, web_contents.get(), profile(),
                          browser_window_interface_.get());
    return web_contents;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_window_interface_;
  BrowserWindowFeatures browser_window_features_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  ui::UnownedUserDataHost active_tab_user_data_host_;
  std::unique_ptr<testing::NiceMock<MockTabListInterface>> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  tabs::MockTabInterface mock_active_tab_;
  raw_ptr<MockAutocompleteController> mock_autocomplete_controller_ = nullptr;
};

TEST_F(WebuiOmniboxHandlerTabScopingTest, QueryAutocomplete_ActiveTabMatches) {
  int32_t active_tab_id = mock_active_tab_.GetHandle().raw_value();
  AutocompleteInput input;
  EXPECT_CALL(*mock_autocomplete_controller_, Start(_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&input));

  handler_->QueryAutocomplete(
      1, active_tab_id, u"active search query",
      /*prevent_inline_autocomplete=*/false, 19,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  EXPECT_EQ(input.text(), u"active search query");
  EXPECT_EQ(test_omnibox_view_->GetText(), u"active search query");
}

TEST_F(WebuiOmniboxHandlerTabScopingTest,
       QueryAutocomplete_NulloptTabId_AcceptedAsActiveTab) {
  AutocompleteInput input;
  EXPECT_CALL(*mock_autocomplete_controller_, Start(_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&input));

  handler_->QueryAutocomplete(
      2, /*tab_id=*/std::nullopt, u"nullopt tab query",
      /*prevent_inline_autocomplete=*/false, 17,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  EXPECT_EQ(input.text(), u"nullopt tab query");
  EXPECT_EQ(test_omnibox_view_->GetText(), u"nullopt tab query");
}

TEST_F(WebuiOmniboxHandlerTabScopingTest,
       QueryAutocomplete_StartsAutocompleteWhenFullPopupDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kWebUIOmniboxFullPopup);

  AutocompleteInput input;
  EXPECT_CALL(*mock_autocomplete_controller_, Start(_))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&input));

  handler_->QueryAutocomplete(
      3, /*tab_id=*/std::nullopt, u"non full popup query",
      /*prevent_inline_autocomplete=*/false, 20,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  EXPECT_EQ(input.text(), u"non full popup query");
}

TEST_F(WebuiOmniboxHandlerTabScopingTest,
       QueryAutocomplete_BackgroundTabMismatched) {
  tabs::MockTabInterface mock_background_tab;
  auto background_web_contents = CreateBackgroundTab(mock_background_tab);

  OmniboxEditModel::State model_state(
      /*user_input_in_progress=*/true,
      /*user_text=*/u"old background query",
      /*keyword=*/u"",
      /*keyword_placeholder=*/u"",
      /*keyword_state=*/KeywordState::kNone,
      /*keyword_mode_entry_method=*/
      metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID,
      /*focus_state=*/OmniboxFocusState::OMNIBOX_FOCUS_VISIBLE,
      /*autocomplete_input=*/AutocompleteInput());
  background_web_contents->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(model_state, gfx::Range(0, 20),
                                     gfx::Range::InvalidRange()));

  int32_t background_tab_id = mock_background_tab.GetHandle().raw_value();

  // Active foreground `AutocompleteController` should not be started.
  EXPECT_CALL(*mock_autocomplete_controller_, Start(_)).Times(0);

  // Initialize `TestOmniboxView` with active foreground text.
  test_omnibox_view_->SetWindowTextAndCaretPos(u"foreground text", 15,
                                               /*update_popup=*/false,
                                               /*notify_text_changed=*/false);
  handler_->QueryAutocomplete(
      42, background_tab_id, u"new background query",
      /*prevent_inline_autocomplete=*/false, 20,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  // Background tab state should be updated.
  auto* background_state = static_cast<OmniboxState*>(
      background_web_contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  ASSERT_TRUE(background_state);
  EXPECT_EQ(background_state->model_state.user_text, u"new background query");
  EXPECT_TRUE(background_state->model_state.user_input_in_progress);

  // Foreground view text should remain untouched.
  EXPECT_EQ(test_omnibox_view_->GetText(), u"foreground text");
}

TEST_F(WebuiOmniboxHandlerTabScopingTest, QueryAutocomplete_InvalidTabHandle) {
  EXPECT_CALL(*mock_autocomplete_controller_, Start(_)).Times(0);

  // Initialize `TestOmniboxView` with active foreground text.
  test_omnibox_view_->SetWindowTextAndCaretPos(u"foreground untouched", 20,
                                               /*update_popup=*/false,
                                               /*notify_text_changed=*/false);

  // Pass an invalid tab ID (e.g. 999999) that cannot be resolved via TabHandle.
  int32_t invalid_tab_id = 999999;
  handler_->QueryAutocomplete(
      99, invalid_tab_id, u"stale tab query",
      /*prevent_inline_autocomplete=*/false, 15,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false,
      /*keyword=*/"", searchbox::mojom::InputMethod::kKeyboard);

  // Foreground view remains untouched.
  EXPECT_EQ(test_omnibox_view_->GetText(), u"foreground untouched");
}

#endif

namespace {
class DeletingWebContentsDelegate : public content::WebContentsDelegate {
 public:
  DeletingWebContentsDelegate() = default;
  ~DeletingWebContentsDelegate() override = default;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)> callback) override {
    if (on_open_url_callback_) {
      std::move(on_open_url_callback_).Run();
    }
    return nullptr;
  }

  void set_on_open_url_callback(base::OnceClosure callback) {
    on_open_url_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure on_open_url_callback_;
};

// A concrete implementation of SearchboxOmniboxClient for testing.
// SearchboxOmniboxClient is abstract because it does not implement
// GetPageClassification().
class TestSearchboxOmniboxClient : public SearchboxOmniboxClient {
 public:
  using SearchboxOmniboxClient::SearchboxOmniboxClient;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::NTP_REALBOX;
  }
};
}  // namespace

// Tests the navigation logic within SearchboxOmniboxClient, specifically
// focusing on edge cases like synchronous object destruction.
class SearchboxOmniboxClientNavigationTest : public SearchboxHandlerTest {
 public:
  SearchboxOmniboxClientNavigationTest() = default;
  ~SearchboxOmniboxClientNavigationTest() override = default;

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;

  void SetUp() override {
    SearchboxHandlerTest::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }
};

TEST_F(SearchboxOmniboxClientNavigationTest,
       OnAutocompleteAccept_HandleSynchronousClientDestruction) {
  auto client = std::make_unique<TestSearchboxOmniboxClient>(
      profile(), web_contents_.get());

  DeletingWebContentsDelegate delegate;
  web_contents_->SetDelegate(&delegate);
  delegate.set_on_open_url_callback(
      base::BindLambdaForTesting([&]() { client.reset(); }));

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.destination_url = GURL("https://google.com");

  // This should NOT crash.
  client->OnAutocompleteAccept(
      match.destination_url, /*post_content=*/nullptr,
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, match.type,
      base::TimeTicks::Now(),
      /*destination_url_entered_without_scheme=*/false,
      /*destination_url_entered_with_http_scheme=*/false,
      /*text=*/u"google", match, /*alternative_nav_match=*/AutocompleteMatch());
}

// OmniboxComposeboxHandler is dedicated to the desktop Omnibox Popup and out of
// scope for Android WebUI NTP.
#if !BUILDFLAG(IS_ANDROID)

class OmniboxComposeboxHandlerTest : public SearchboxHandlerTest {
 public:
  OmniboxComposeboxHandlerTest() = default;
  ~OmniboxComposeboxHandlerTest() override = default;

  void OpenUrl(GURL url, WindowOpenDisposition disposition) {
    handler_->ProcessContextAndOpenUrl(url, disposition);
  }

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    SetupMockBrowserWindowInterface(
        browser_window_interface_, profile(), browser_window_features_,
        unowned_user_data_host_, &mock_tab_, web_contents_.get());

    tab_list_ = std::make_unique<testing::NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);

    SetupMockTabListInterface(*tab_list_, &mock_tab_);
    SetupMockTabInterface(mock_tab_, web_contents_.get(), profile());

    auto mock_context_controller = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchContextController>>();
    auto* service = ContextualSearchServiceFactory::GetForProfile(profile());
    session_handle_ = service->CreateSessionForTesting(
        std::move(mock_context_controller), /*metrics_recorder=*/nullptr);
    auto client = std::make_unique<TestOmniboxClient>();
    omnibox_controller_ =
        std::make_unique<OmniboxController>(std::move(client), std::nullopt);
    test_omnibox_view_ =
        std::make_unique<TestOmniboxView>(omnibox_controller_.get());

    OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents_.get());
    OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get())
        ->set_omnibox_controller(omnibox_controller_.get());

    handler_ = std::make_unique<OmniboxComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        searchbox_page_.BindAndGetRemote(), profile(), web_contents_.get(),
        base::BindLambdaForTesting(
            [this]() -> contextual_search::ContextualSearchSessionHandle* {
              return session_handle_.get();
            }),
        base::BindLambdaForTesting([]() {}));

    handler_->set_delegate(&mock_delegate_);
  }

  void TearDown() override {
    handler_.reset();
    if (web_contents_) {
      if (auto* helper = OmniboxPopupWebContentsHelper::FromWebContents(
              web_contents_.get())) {
        helper->set_omnibox_controller(nullptr);
      }
    }
    test_omnibox_view_.reset();
    omnibox_controller_.reset();
    session_handle_.reset();
    tab_list_registration_.reset();
    tab_list_.reset();
    web_contents_.reset();
    SearchboxHandlerTest::TearDown();
  }

 protected:
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  BrowserWindowFeatures browser_window_features_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<testing::NiceMock<MockTabListInterface>> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  tabs::MockTabInterface mock_tab_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockSearchboxPage> searchbox_page_;
  testing::NiceMock<MockSearchboxHandlerDelegate> mock_delegate_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<TestOmniboxView> test_omnibox_view_;
  std::unique_ptr<OmniboxComposeboxHandler> handler_;

  std::unique_ptr<OmniboxComposeboxHandler> CreateHandler(
      MockSearchboxPage& page) {
    return std::make_unique<OmniboxComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page.BindAndGetRemote(), profile(), web_contents_.get(),
        base::BindLambdaForTesting(
            [this]() -> contextual_search::ContextualSearchSessionHandle* {
              return session_handle_.get();
            }),
        base::BindLambdaForTesting([]() {}));
  }
};

TEST_F(OmniboxComposeboxHandlerTest, OpenUrl_StopsAutocomplete) {
  auto mock_autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  omnibox_controller_->SetAutocompleteControllerForTesting(
      std::move(mock_autocomplete_controller));

  EXPECT_CALL(mock_delegate_, GetOmniboxController())
      .WillRepeatedly(testing::Return(omnibox_controller_.get()));

  OpenUrl(GURL("https://example.com"), WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(omnibox_controller_->autocomplete_controller()->done());
}

TEST_F(OmniboxComposeboxHandlerTest, ContentSharingPolicy) {
  handler_.reset();
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  testing::NiceMock<MockSearchboxPage> local_searchbox_page;

  EXPECT_CALL(local_searchbox_page, UpdateContentSharingPolicy(true)).Times(1);

  auto local_handler = CreateHandler(local_searchbox_page);

  task_environment_.RunUntilIdle();

  // Now change pref to false.
  EXPECT_CALL(local_searchbox_page, UpdateContentSharingPolicy(false)).Times(1);
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  task_environment_.RunUntilIdle();

  // Change pref back to true.
  EXPECT_CALL(local_searchbox_page, UpdateContentSharingPolicy(true)).Times(1);
  profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  task_environment_.RunUntilIdle();
}

TEST_F(OmniboxComposeboxHandlerTest, OpenLensSearch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kWebUIOmniboxAskGAboutThisPage,
      {{"Omnibox_AskGLensChipRoute", "true"}});

  auto client = std::make_unique<MockAutocompleteProviderClient>();
  auto* client_ptr = client.get();
  EXPECT_CALL(*client_ptr,
              OpenLensOverlay(
                  true, lens::LensOverlayInvocationSource::kOmniboxPopupButton))
      .Times(1);

  auto autocomplete_controller = std::make_unique<AutocompleteController>(
      std::move(client), AutocompleteControllerConfig{});
  omnibox_controller_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  omnibox_controller_->edit_model()->SetUserText(u"query in progress");
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());

  handler_->OpenLensSearch();

  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());
}

TEST_F(OmniboxComposeboxHandlerTest, LensSearchEligibility) {
  handler_.reset();

  auto client = std::make_unique<MockAutocompleteProviderClient>();
  auto* client_ptr = client.get();
  EXPECT_CALL(*client_ptr, IsLensEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_ptr, AreLensEntrypointsVisible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_ptr, GetPrefs())
      .WillRepeatedly(testing::Return(profile()->GetPrefs()));

  auto autocomplete_controller = std::make_unique<AutocompleteController>(
      std::move(client), AutocompleteControllerConfig{});
  omnibox_controller_->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));

  omnibox_controller_->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  auto run_test_case =
      [&](const GURL& url,
          metrics::OmniboxEventProto::PageClassification classification,
          bool expected_eligible) {
        testing::NiceMock<MockSearchboxPage> local_searchbox_page;

        auto* test_client =
            static_cast<TestOmniboxClient*>(omnibox_controller_->client());
        test_client->SetURL(url);
        test_client->location_bar_model()->set_page_classification(
            classification);

        EXPECT_CALL(local_searchbox_page,
                    UpdateLensSearchEligibility(expected_eligible))
            .Times(1);

        auto local_handler = CreateHandler(local_searchbox_page);

        task_environment_.RunUntilIdle();
      };

  run_test_case(GURL("https://foo.com"), metrics::OmniboxEventProto::OTHER,
                true);
  run_test_case(GURL("chrome://newtab"), metrics::OmniboxEventProto::NTP,
                false);
  run_test_case(GURL("chrome://settings"), metrics::OmniboxEventProto::OTHER,
                false);
}
#endif
