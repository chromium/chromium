// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search/ntp_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
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
#include "ui/base/webui/web_ui_util.h"

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

  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    handler_ = std::make_unique<RealboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        web_contents_.get());
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    SearchboxHandlerTest::TearDown();
    handler_.reset();
  }
};

TEST_F(RealboxHandlerTest, RealboxLensVariationsContainsVariations) {
  SearchboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                         profile());

  EXPECT_EQ("CGQ", *source()->GetLocalizedStrings()->FindString(
                       "searchboxLensVariations"));
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

    handler_->QueryAutocomplete(u"", /*prevent_inline_autocomplete=*/false);

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

    handler_->QueryAutocomplete(u"a", /*prevent_inline_autocomplete=*/false);

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

TEST_F(RealboxHandlerTest, GetPlaceholderConfig) {
  base::test::TestFuture<searchbox::mojom::PlaceholderConfigPtr> future;
  handler_->GetPlaceholderConfig(future.GetCallback());
  auto config = future.Take();

  ASSERT_GT(config->texts.size(), 0u);
  ASSERT_EQ(config->change_text_animation_interval.InMilliseconds(), 2000u);
  ASSERT_EQ(config->fade_text_animation_duration.InMilliseconds(), 250u);
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
  handler_->AddFileContextFromBrowser(token, file_info.Clone());
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
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        /*web_contents=*/nullptr, lens_searchbox_client_.get());

    handler_->SetPage(page_.BindAndGetRemote());
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

    handler_->QueryAutocomplete(u"", /*prevent_inline_autocomplete=*/false);

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

    handler_->QueryAutocomplete(u"a", /*prevent_inline_autocomplete=*/false);

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

    const char search_icon[] = "//resources/images/icon_search.svg";
    const std::string& svg_name = handler_->AutocompleteIconToResourceName(
        omnibox::kSubdirectoryArrowRightIcon);

    EXPECT_EQ(svg_name, search_icon);
  }
}

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

    handler_ = std::make_unique<WebuiOmniboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        /*metrics_reporter=*/nullptr, omnibox_controller_.get(), &web_ui_);
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    handler_.reset();
    SearchboxHandlerTest::TearDown();
  }

  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<OmniboxPopupUI> omnibox_popup_ui_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
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

TEST_F(WebuiOmniboxHandlerTest, OnShow) {
  EXPECT_CALL(page_, OnShow());
  handler_->OnShow();
  page_.FlushForTesting();
}
