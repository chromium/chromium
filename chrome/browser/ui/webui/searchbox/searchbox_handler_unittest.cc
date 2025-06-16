// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "searchbox_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/search/ntp_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "lens_searchbox_handler.h"
#include "realbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

namespace {

class TestObserver : public OmniboxWebUIPopupChangeObserver {
 public:
  void OnPopupElementSizeChanged(gfx::Size size) override { called_ = true; }
  bool called() const { return called_; }

 private:
  bool called_ = false;
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
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  void SetUp() override {
    source_ = content::TestWebUIDataSource::Create("test-data-source");

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    ASSERT_EQ(
        variations::VariationsIdsProvider::ForceIdsResult::SUCCESS,
        variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
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
  std::unique_ptr<RealboxHandler> handler_;

 private:
  void SetUp() override {
    SearchboxHandlerTest::SetUp();

    handler_ = std::make_unique<RealboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(), profile(),
        /*web_contents=*/nullptr, /*metrics_reporter=*/nullptr,
        /*omnibox_controller=*/nullptr);
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    SearchboxHandlerTest::TearDown();
    handler_.reset();
  }
};

TEST_F(RealboxHandlerTest, RealboxLensVariationsContainsVariations) {
  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_EQ("CGQ", *source()->GetLocalizedStrings()->FindString(
                       "searchboxLensVariations"));
}

TEST_F(RealboxHandlerTest, RealboxUpdatesSelection) {
  searchbox::mojom::OmniboxPopupSelectionPtr old_selection;
  searchbox::mojom::OmniboxPopupSelectionPtr selection;
  EXPECT_CALL(page_, UpdateSelection)
      .Times(4)
      .WillRepeatedly(
          testing::Invoke([&old_selection, &selection](
                              searchbox::mojom::OmniboxPopupSelectionPtr arg0,
                              searchbox::mojom::OmniboxPopupSelectionPtr arg1) {
            old_selection = std::move(arg0);
            selection = std::move(arg1);
          }));

  handler_->UpdateSelection(
      OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch),
      OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL));
  page_.FlushForTesting();
  EXPECT_EQ(0, selection->line);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kNormal, selection->state);

  handler_->UpdateSelection(
      OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL),
      OmniboxPopupSelection(1, OmniboxPopupSelection::KEYWORD_MODE));
  page_.FlushForTesting();
  EXPECT_EQ(1, selection->line);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kKeywordMode,
            selection->state);

  handler_->UpdateSelection(
      OmniboxPopupSelection(2, OmniboxPopupSelection::NORMAL),
      OmniboxPopupSelection(2, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
                            4));
  page_.FlushForTesting();
  EXPECT_EQ(2, selection->line);
  EXPECT_EQ(4, selection->action_index);
  EXPECT_EQ(searchbox::mojom::SelectionLineState::kFocusedButtonAction,
            selection->state);

  handler_->UpdateSelection(
      OmniboxPopupSelection(3, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION, 4),
      OmniboxPopupSelection(
          3, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION));
  page_.FlushForTesting();
  EXPECT_EQ(3, selection->line);
  EXPECT_EQ(
      searchbox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion,
      selection->state);
}

TEST_F(RealboxHandlerTest, RealboxObservationWorks) {
  TestObserver observer;
  EXPECT_FALSE(observer.called());
  handler_->AddObserver(&observer);
  EXPECT_TRUE(handler_->HasObserver(&observer));
  handler_->RemoveObserver(&observer);
  EXPECT_FALSE(handler_->HasObserver(&observer));
  EXPECT_TRUE(observer.called());
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
          handler_->omnibox_controller(),
          /*view=*/nullptr);
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    SCOPED_TRACE("Empty input");

    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input_text)));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input)));

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
        .WillOnce(DoAll(SaveArg<0>(&input_text)));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input)));

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
        /*web_contents=*/nullptr, /*metrics_reporter=*/nullptr,
        lens_searchbox_client_.get());

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
          handler_->omnibox_controller(),
          /*view=*/nullptr);
  omnibox_edit_model_ = omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  {
    SCOPED_TRACE("Empty input");

    std::u16string input_text;
    EXPECT_CALL(*omnibox_edit_model_, SetUserText(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input_text)));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input)));

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
        .WillRepeatedly(ReturnRef(suggest_inputs));

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
        .WillOnce(DoAll(SaveArg<0>(&input_text)));

    AutocompleteInput input;
    EXPECT_CALL(*autocomplete_controller_, Start(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&input)));

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
        .WillRepeatedly(ReturnRef(suggest_inputs));

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
}
