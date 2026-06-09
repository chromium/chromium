// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/test_omnibox_view.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "content/public/test/web_contents_tester.h"
#include "lens_searchbox_handler.h"
#include "realbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

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

class RealboxHandlerTest : public SearchboxHandlerTest {
 public:
  RealboxHandlerTest() = default;

  RealboxHandlerTest(const RealboxHandlerTest&) = delete;
  RealboxHandlerTest& operator=(const RealboxHandlerTest&) = delete;
  ~RealboxHandlerTest() override = default;

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<RealboxHandler> handler_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    webui::SetBrowserWindowInterface(web_contents_.get(),
                                     &browser_window_interface_);

    handler_ = std::make_unique<RealboxHandler>(
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

TEST_F(RealboxHandlerTest, OnEmbeddedPermissionPromptChanged) {
  MockSearchboxHandlerDelegate delegate;
  handler_->set_delegate(&delegate);

  // Case: is_showing=true, non-zero size (adds buffer)
  {
    EXPECT_CALL(page_, OnEmbeddedPermissionPromptChanged(
                           true, gfx::Size(100 + 40, 200 + 40)));
    EXPECT_CALL(delegate, OnEmbeddedPermissionDialogChanged(
                              true, gfx::Size(100 + 40, 200 + 40)));
    handler_->OnPermissionPromptChanged(true, gfx::Size(100, 200));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=true, zero size (does not add buffer)
  {
    EXPECT_CALL(page_,
                OnEmbeddedPermissionPromptChanged(true, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(true, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(true, gfx::Size(0, 0));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=false, non-zero size (does not add buffer, size is 0, 0)
  {
    EXPECT_CALL(page_,
                OnEmbeddedPermissionPromptChanged(false, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(false, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(false, gfx::Size(100, 200));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Case: is_showing=false, zero size (does not add buffer, size is 0, 0)
  {
    EXPECT_CALL(page_,
                OnEmbeddedPermissionPromptChanged(false, gfx::Size(0, 0)));
    EXPECT_CALL(delegate,
                OnEmbeddedPermissionDialogChanged(false, gfx::Size(0, 0)));
    handler_->OnPermissionPromptChanged(false, gfx::Size(0, 0));
    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
    testing::Mock::VerifyAndClearExpectations(&delegate);
  }
}

TEST_F(RealboxHandlerTest, ShouldShowDriveDisclaimer) {
  base::test::TestFuture<bool> future;
  handler_->ShouldShowDriveDisclaimer(future.GetCallback());
  EXPECT_FALSE(future.Take());
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
  handler_->omnibox_controller()->SetAutocompleteControllerForTesting(
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

    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input_text));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&input));

    handler_->QueryAutocomplete(u"", /*prevent_inline_autocomplete=*/false, 0);

    EXPECT_EQ(input_text, u"");
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

    handler_->QueryAutocomplete(u"a", /*prevent_inline_autocomplete=*/false, 0);

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

TEST_F(RealboxHandlerTest, GetPlaceholderConfig_NoPecApiReturnsEmpty) {
  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetPlaceholderConfig(future.GetCallback());
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

class SearchboxHandlerPecApiTest : public RealboxHandlerTest {
 public:
  SearchboxHandlerPecApiTest() = default;
  ~SearchboxHandlerPecApiTest() override = default;

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
    handler_ = std::make_unique<RealboxHandler>(
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

TEST_F(SearchboxHandlerPecApiTest, GetPlaceholderConfig_WithToolConfigs) {
  omnibox::SearchboxConfig& config = mock_aim_eligibility_service_->config();

  auto* tool = config.add_tool_configs();
  tool->set_tool(omnibox::TOOL_MODE_IMAGE_GEN);

  auto* tool2 = config.add_tool_configs();
  tool2->set_tool(omnibox::TOOL_MODE_CANVAS);

  ON_CALL(*mock_aim_eligibility_service_, GetSearchboxConfig())
      .WillByDefault(testing::Return(&config));

  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetPlaceholderConfig(future.GetCallback());
  auto result = future.Take();

  ASSERT_EQ(result->texts.size(), 3u);
  EXPECT_EQ(result->texts[0], u"Ask Google");
  EXPECT_EQ(result->texts[1], u"Describe your image");
  EXPECT_EQ(result->texts[2], u"Create anything");
}

TEST_F(SearchboxHandlerPecApiTest,
       GetPlaceholderConfig_NoToolConfigsReturnsEmpty) {
  omnibox::SearchboxConfig& config = mock_aim_eligibility_service_->config();

  ON_CALL(*mock_aim_eligibility_service_, GetSearchboxConfig())
      .WillByDefault(testing::Return(&config));

  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetPlaceholderConfig(future.GetCallback());
  auto result = future.Take();

  // Not eligible tools -> cycling disabled -> empty placeholder list.
  ASSERT_EQ(result->texts.size(), 0u);
}

TEST_F(SearchboxHandlerPecApiTest,
       GetPlaceholderConfig_NullSearchboxConfigReturnsEmpty) {
  ON_CALL(*mock_aim_eligibility_service_, GetSearchboxConfig())
      .WillByDefault(testing::Return(nullptr));

  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetPlaceholderConfig(future.GetCallback());
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

class LensSearchboxHandlerTest : public SearchboxHandlerTest {
 public:
  LensSearchboxHandlerTest() = default;

  LensSearchboxHandlerTest(const LensSearchboxHandlerTest&) = delete;
  LensSearchboxHandlerTest& operator=(const LensSearchboxHandlerTest&) = delete;
  ~LensSearchboxHandlerTest() override = default;

 protected:
  std::unique_ptr<testing::NiceMock<MockLensSearchboxClient>>
      lens_searchbox_client_;
  std::unique_ptr<LensSearchboxHandler> handler_;

 private:
  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    // Set a mock LensSearchboxClient.
    lens_searchbox_client_ =
        std::make_unique<testing::NiceMock<MockLensSearchboxClient>>();

    handler_ = std::make_unique<LensSearchboxHandler>(
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
  handler_->omnibox_controller()->SetAutocompleteControllerForTesting(
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

    handler_->QueryAutocomplete(u"", /*prevent_inline_autocomplete=*/false, 0);

    EXPECT_EQ(input_text, u"");
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

    handler_->QueryAutocomplete(u"a", /*prevent_inline_autocomplete=*/false, 0);

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

    handler_ = std::make_unique<WebuiOmniboxHandler>(
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
  std::unique_ptr<WebuiOmniboxHandler> handler_;
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
  feature_list.InitAndEnableFeature(omnibox::kWebUIOmniboxFullPopupV2);
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
      match, 0, omnibox_controller_->edit_model(), bookmark_model,
      omnibox::GroupConfigMap(),
      omnibox_controller_->client()->GetTemplateURLService());

  ASSERT_TRUE(mojom_match.has_value());
  EXPECT_EQ(mojom_match.value()->icon_path,
            searchbox_internal::kReplyRotated180IconResourceName);
}

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

namespace {
class MockPage : public composebox::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<composebox::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<composebox::mojom::Page> receiver_{this};
};
}  // namespace

class OmniboxComposeboxHandlerTest : public SearchboxHandlerTest {
 public:
  OmniboxComposeboxHandlerTest() = default;
  ~OmniboxComposeboxHandlerTest() override = default;

  void OpenUrl(GURL url, WindowOpenDisposition disposition) {
    handler_->OpenUrl(url, disposition);
  }

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    auto mock_controller = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchContextController>>();
    auto* service = ContextualSearchServiceFactory::GetForProfile(profile());
    session_handle_ = service->CreateSessionForTesting(
        std::move(mock_controller), /*metrics_recorder=*/nullptr);

    handler_ = std::make_unique<OmniboxComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        page_.BindAndGetRemote(),
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
    session_handle_.reset();
    web_contents_.reset();
    SearchboxHandlerTest::TearDown();
  }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockPage> page_;
  testing::NiceMock<MockSearchboxPage> searchbox_page_;
  testing::NiceMock<MockSearchboxHandlerDelegate> mock_delegate_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::unique_ptr<OmniboxComposeboxHandler> handler_;
};

TEST_F(OmniboxComposeboxHandlerTest, OpenUrl_StopsAutocomplete) {
  auto client = std::make_unique<TestOmniboxClient>();
  auto omnibox_controller =
      std::make_unique<OmniboxController>(std::move(client), std::nullopt);

  auto mock_autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::make_unique<MockAutocompleteProviderClient>(), 0);
  omnibox_controller->SetAutocompleteControllerForTesting(
      std::move(mock_autocomplete_controller));

  EXPECT_CALL(mock_delegate_, GetOmniboxController())
      .WillRepeatedly(testing::Return(omnibox_controller.get()));

  OpenUrl(GURL("https://example.com"), WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(omnibox_controller->autocomplete_controller()->done());
}
