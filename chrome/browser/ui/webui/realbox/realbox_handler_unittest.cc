// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "realbox_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPage : public omnibox::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<omnibox::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<omnibox::mojom::Page> receiver_{this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              AutocompleteResultChanged,
              (omnibox::mojom::AutocompleteResultPtr));
  MOCK_METHOD(void,
              UpdateSelection,
              (omnibox::mojom::OmniboxPopupSelectionPtr));
};

class TestObserver : public OmniboxWebUIPopupChangeObserver {
 public:
  void OnPopupElementSizeChanged(gfx::Size size) override { called_ = true; }
  bool called() const { return called_; }

 private:
  bool called_ = false;
};

}  // namespace

class RealboxHandlerTest : public ::testing::Test {
 public:
  RealboxHandlerTest() = default;

  RealboxHandlerTest(const RealboxHandlerTest&) = delete;
  RealboxHandlerTest& operator=(const RealboxHandlerTest&) = delete;
  ~RealboxHandlerTest() override = default;

  content::TestWebUIDataSource* source() { return source_.get(); }
  TestingProfile* profile() { return profile_.get(); }

 protected:
  std::unique_ptr<RealboxHandler> handler_;
  testing::NiceMock<MockPage> page_;

 private:
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

    handler_ = std::make_unique<RealboxHandler>(
        mojo::PendingReceiver<omnibox::mojom::PageHandler>(), profile(),
        /*web_contents=*/nullptr, /*metrics_reporter=*/nullptr,
        /*omnibox_controller=*/nullptr);
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override { handler_.reset(); }
};

TEST_F(RealboxHandlerTest, RealboxLensSearchIsFalseWhenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{},
      /*disabled_features=*/{ntp_features::kNtpRealboxLensSearch});

  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_FALSE(
      source()->GetLocalizedStrings()->FindBool("realboxLensSearch").value());
}

TEST_F(RealboxHandlerTest, RealboxLensSearchIsTrueWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{ntp_features::kNtpRealboxLensSearch, {{}}}},
      /*disabled_features=*/{});

  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_TRUE(
      source()->GetLocalizedStrings()->FindBool("realboxLensSearch").value());
}

TEST_F(RealboxHandlerTest, RealboxLensVariationsContainsVariations) {
  RealboxHandler::SetupWebUIDataSource(source()->GetWebUIDataSource(),
                                       profile());

  EXPECT_EQ("CGQ", *source()->GetLocalizedStrings()->FindString(
                       "realboxLensVariations"));
}

TEST_F(RealboxHandlerTest, RealboxUpdatesSelection) {
  omnibox::mojom::OmniboxPopupSelectionPtr selection;
  EXPECT_CALL(page_, UpdateSelection)
      .Times(4)
      .WillRepeatedly(testing::Invoke(
          [&selection](omnibox::mojom::OmniboxPopupSelectionPtr arg) {
            selection = std::move(arg);
          }));

  handler_->UpdateSelection(
      OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL));
  page_.FlushForTesting();
  EXPECT_EQ(0, selection->line);
  EXPECT_EQ(omnibox::mojom::SelectionLineState::kNormal, selection->state);

  handler_->UpdateSelection(
      OmniboxPopupSelection(1, OmniboxPopupSelection::KEYWORD_MODE));
  page_.FlushForTesting();
  EXPECT_EQ(1, selection->line);
  EXPECT_EQ(omnibox::mojom::SelectionLineState::kKeywordMode, selection->state);

  handler_->UpdateSelection(OmniboxPopupSelection(
      2, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION, 4));
  page_.FlushForTesting();
  EXPECT_EQ(2, selection->line);
  EXPECT_EQ(4, selection->action_index);
  EXPECT_EQ(omnibox::mojom::SelectionLineState::kFocusedButtonAction,
            selection->state);

  handler_->UpdateSelection(OmniboxPopupSelection(
      3, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION));
  page_.FlushForTesting();
  EXPECT_EQ(3, selection->line);
  EXPECT_EQ(omnibox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion,
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
