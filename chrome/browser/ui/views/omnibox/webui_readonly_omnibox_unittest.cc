// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::ElementsAre;

MATCHER_P2(IsSpan, expect_text, expect_color, "") {
  EXPECT_EQ(arg->text, expect_text);
  EXPECT_EQ(arg->color, expect_color);
  EXPECT_FALSE(arg->strikethrough);
  return true;
}

MATCHER_P2(IsStrikethrough, expect_text, expect_color, "") {
  EXPECT_EQ(arg->text, expect_text);
  EXPECT_EQ(arg->color, expect_color);
  EXPECT_TRUE(arg->strikethrough);
  return true;
}

using toolbar_ui_api::mojom::OmniboxTextColor;

class TestUpdatePropagator : public WebUIReadOnlyOmnibox::UpdatePropagator {
 public:
  ~TestUpdatePropagator() override = default;
  void PropagateOmniboxUpdate(
      toolbar_ui_api::mojom::OmniboxViewStatePtr update) override {
    state_ = std::move(update);
  }

  void PropagateFocusRequest(
      toolbar_ui_api::mojom::FocusRequestTarget target) override {}

  toolbar_ui_api::mojom::OmniboxViewStatePtr TakeState() {
    return std::move(state_);
  }

 private:
  toolbar_ui_api::mojom::OmniboxViewStatePtr state_;
};

class WebUIReadOnlyOmniboxTest : public testing::Test {
 protected:
  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  content::BrowserTaskEnvironment browser_threads_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  TestUpdatePropagator update_propagator_;
  std::unique_ptr<WebUIReadOnlyOmnibox> omnibox_view_;

  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> wc1_;
  raw_ptr<content::WebContents> wc2_;
};

void WebUIReadOnlyOmniboxTest::SetUp() {
  profile_ = TestingProfile::Builder().Build();

  auto omnibox_client = std::make_unique<TestOmniboxClient>();
  omnibox_client_ = omnibox_client.get();
  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::move(omnibox_client));

  EXPECT_CALL(*omnibox_client_, GetPrefs())
      .WillRepeatedly(testing::Return(profile_->GetPrefs()));

  omnibox_view_ = std::make_unique<WebUIReadOnlyOmnibox>(
      omnibox_controller_.get(), update_propagator_);

  wc1_ = web_contents_factory_.CreateWebContents(profile_.get());
  wc2_ = web_contents_factory_.CreateWebContents(profile_.get());
}

void WebUIReadOnlyOmniboxTest::TearDown() {
  web_contents_factory_.DestroyWebContents(wc1_.ExtractAsDangling());
  web_contents_factory_.DestroyWebContents(wc2_.ExtractAsDangling());
}

TEST_F(WebUIReadOnlyOmniboxTest, StateManagement) {
  std::u16string partial = u"https://uk.wikipe";
  omnibox_view_->SetUserText(partial);
  omnibox_view_->SetCaretPos(partial.size());
  EXPECT_FALSE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(partial, omnibox_view_->GetText());
  omnibox_view_->SaveStateToTab(wc1_);

  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  ASSERT_EQ(2u, mojo_state->text_pieces.size());
  EXPECT_TRUE(mojo_state->text_is_url);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("uk.wikipe", OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(gfx::Range(partial.size()), mojo_state->selection);

  std::u16string complete = u"https://chromium.org";
  omnibox_view_->SetUserText(complete);
  omnibox_view_->SelectAll(/*reversed=*/false);
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(complete, omnibox_view_->GetText());
  omnibox_view_->SaveStateToTab(wc2_);

  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("chromium.org", OmniboxTextColor::kOmniboxText)));
  ASSERT_EQ(2u, mojo_state->text_pieces.size());
  EXPECT_EQ(gfx::Range(0, complete.size()), mojo_state->selection);
  EXPECT_TRUE(mojo_state->text_is_url);

  // Emulate switching back to wc1.
  omnibox_view_->OnTabChanged(wc1_);
  EXPECT_FALSE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(partial, omnibox_view_->GetText());
  EXPECT_EQ(omnibox_view_->GetSelectionBounds(), gfx::Range(partial.size()));

  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("uk.wikipe", OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(gfx::Range(partial.size()), mojo_state->selection);
  EXPECT_TRUE(mojo_state->text_is_url);

  // Switch back to wc2.
  omnibox_view_->OnTabChanged(wc2_);
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(complete, omnibox_view_->GetText());
  EXPECT_EQ(omnibox_view_->GetSelectionBounds(),
            gfx::Range(0, complete.size()));

  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("chromium.org", OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(gfx::Range(0, complete.size()), mojo_state->selection);
  EXPECT_TRUE(mojo_state->text_is_url);

  // If no saved state, pulls from the location bar model.
  std::u16string navigated_to = u"https://developer.mozilla.org/";
  location_bar_model()->set_url_for_display(navigated_to);
  omnibox_view_->ResetTabState(wc2_);
  omnibox_view_->OnTabChanged(wc2_);
  EXPECT_EQ(navigated_to, omnibox_view_->GetText());
  EXPECT_EQ(gfx::Range(), omnibox_view_->GetSelectionBounds());

  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(
          IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
          IsSpan("developer.mozilla.org", OmniboxTextColor::kOmniboxText),
          IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_EQ(gfx::Range(0, 0), mojo_state->selection);
  EXPECT_TRUE(mojo_state->text_is_url);

  // Update() can pull further changes.
  std::u16string navigated_to2 = u"https://developer.mozilla.org/en-US";
  location_bar_model()->set_url_for_display(navigated_to2);
  omnibox_view_->Update();
  EXPECT_EQ(navigated_to2, omnibox_view_->GetText());

  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(
          IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
          IsSpan("developer.mozilla.org", OmniboxTextColor::kOmniboxText),
          IsSpan("/en-US", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_EQ(gfx::Range(0, 0), mojo_state->selection);
  EXPECT_TRUE(mojo_state->text_is_url);

  // Can also specify user-entered stuff (which is not a URL).
  omnibox_view_->SetUserText(u"Searching for stuff");
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("Searching for stuff",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(gfx::Range(19, 19), mojo_state->selection);
  EXPECT_FALSE(mojo_state->text_is_url);
}

TEST_F(WebUIReadOnlyOmniboxTest, GetOmniboxTextLength) {
  std::u16string partial = u"https://uk.wikipe";
  omnibox_view_->SetUserText(partial);
  EXPECT_EQ(partial.size(),
            static_cast<size_t>(omnibox_view_->GetOmniboxTextLength()));
}

TEST_F(WebUIReadOnlyOmniboxTest, SSLError) {
  location_bar_model()->set_cert_status(net::CERT_STATUS_REVOKED);

  // https + dangerous: red, crossed out.
  location_bar_model()->set_url(GURL("https://broken.example.org/"));
  location_bar_model()->set_security_level(security_state::DANGEROUS);
  omnibox_view_->Update();
  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsStrikethrough(
                      "https", OmniboxTextColor::kOmniboxSecurityChipDangerous),
                  IsSpan("://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("broken.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_TRUE(mojo_state->text_is_url);

  // No dangerous loses the red.
  location_bar_model()->set_security_level(security_state::WARNING);
  omnibox_view_->RevertAll();
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsStrikethrough("https", OmniboxTextColor::kOmniboxText),
                  IsSpan("://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("broken.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_TRUE(mojo_state->text_is_url);

  // Weird scheme + dangerous doesn't get anything special.
  // I wonder how we would get a certificate error in that case?
  location_bar_model()->set_url(GURL("chrome://version"));
  location_bar_model()->set_security_level(security_state::DANGEROUS);
  omnibox_view_->Update();
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("chrome://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("version", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_TRUE(mojo_state->text_is_url);
}

TEST_F(WebUIReadOnlyOmniboxTest, InputVersion) {
  location_bar_model()->set_url(GURL("https://www.example.org/"));
  omnibox_view_->Update();

  // Send some input as if from the WebUI.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://en.wikiped", /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/10, gfx::Range(18))))
          .has_value());

  // State will reflect it, including the version.
  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  // Views omnibox would highlight in this case, but we can't render that
  // when editable anyway, so might as well not spend the cycles.
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("https://en.wikiped",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(1, mojo_state->browser_version);
  EXPECT_EQ(10, mojo_state->ui_version);

  // Resetting the URL should bump the browser version and send the new URL.
  omnibox_view_->RevertAll();
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("www.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_EQ(2, mojo_state->browser_version);
  EXPECT_EQ(0, mojo_state->ui_version);

  // Racing input gets ignored.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://en.wikipedi", /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/11, gfx::Range(19))))
          .has_value());
  mojo_state = update_propagator_.TakeState();
  // Nothing got updated, so update_propagator_ didn't see anything.
  EXPECT_FALSE(mojo_state);
  // We can ask to compute the state explicitly to verify it, however.
  mojo_state = omnibox_view_->ComputeMojoState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("www.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_EQ(2, mojo_state->browser_version);
  EXPECT_EQ(0, mojo_state->ui_version);

  // Now an update with appropriate browser version will work.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://www.example.org/a",
                  /*inline_completion=*/u"",
                  /*browser_version=*/2, /*ui_version=*/1, gfx::Range(25))))
          .has_value());
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  // Views omnibox would highlight in this case, but we can't render that
  // when editable anyway, so might as well not spend the cycles.
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("https://www.example.org/a",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(2, mojo_state->browser_version);
  EXPECT_EQ(1, mojo_state->ui_version);
}

}  // namespace
