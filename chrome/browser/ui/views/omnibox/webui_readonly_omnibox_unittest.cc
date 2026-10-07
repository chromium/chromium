// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using testing::ElementsAre;

MATCHER_P2(IsSpan, expect_text, expect_color, "") {
  EXPECT_EQ(arg->text, base::UTF8ToUTF16(expect_text));
  EXPECT_EQ(arg->color, expect_color);
  EXPECT_FALSE(arg->strikethrough);
  return true;
}

MATCHER_P2(IsStrikethrough, expect_text, expect_color, "") {
  EXPECT_EQ(arg->text, base::UTF8ToUTF16(expect_text));
  EXPECT_EQ(arg->color, expect_color);
  EXPECT_TRUE(arg->strikethrough);
  return true;
}

using toolbar_ui_api::mojom::OmniboxTextColor;

class TestUpdatePropagator : public WebUIReadOnlyOmnibox::UpdatePropagator {
 public:
  ~TestUpdatePropagator() override = default;

  void set_toolbar_delegate(WebUIToolbarControlDelegate* toolbar_delegate) {
    toolbar_delegate_ = toolbar_delegate;
  }

  void PropagateOmniboxUpdate(
      toolbar_ui_api::mojom::OmniboxViewStatePtr update) override {
    state_ = std::move(update);
  }

  void PropagateApplyFocusRingToAimButton(bool force_focus) override {}

  // Mirrors WebUILocationBar::PropagateFocusRequest() without the full popup:
  // the request is forwarded to the toolbar delegate, which synchronously
  // focuses the toolbar's WebView.
  void PropagateFocusRequest(
      toolbar_ui_api::mojom::FocusRequestTarget target) override {
    if (toolbar_delegate_) {
      toolbar_delegate_->OnFocusRequested(target);
    }
  }

  void OpenOmniboxIfFullPopup(bool query_zps) override {}

  toolbar_ui_api::mojom::OmniboxViewStatePtr TakeState() {
    return std::move(state_);
  }

 private:
  raw_ptr<WebUIToolbarControlDelegate> toolbar_delegate_ = nullptr;
  toolbar_ui_api::mojom::OmniboxViewStatePtr state_;
};

class WebUIReadOnlyOmniboxTest : public ChromeViewsTestBase {
 protected:
  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }

  void FocusOmnibox(uint32_t browser_version = 0) {
    widget_->GetContentsView()->RequestFocus();
    EXPECT_TRUE(
        omnibox_view_
            ->OnOmniboxAction(
                toolbar_ui_api::mojom::OmniboxAction::NewFocusChange(
                    toolbar_ui_api::mojom::OmniboxActionFocusChange::New(
                        /*has_focus=*/true,
                        /*request_clear_keyword=*/false,
                        /*activate_default_search=*/false,
                        /*start_zero_suggest=*/false,
                        /*browser_version=*/browser_version,
                        /*selection=*/gfx::Range(0))))
            .has_value());
  }

  void BlurOmnibox(uint32_t browser_version = 0) {
    widget_->GetFocusManager()->ClearFocus();
    EXPECT_TRUE(
        omnibox_view_
            ->OnOmniboxAction(
                toolbar_ui_api::mojom::OmniboxAction::NewFocusChange(
                    toolbar_ui_api::mojom::OmniboxActionFocusChange::New(
                        /*has_focus=*/false,
                        /*request_clear_keyword=*/false,
                        /*activate_default_search=*/false,
                        /*start_zero_suggest=*/false,
                        /*browser_version=*/browser_version,
                        /*selection=*/gfx::Range(0))))
            .has_value());
  }

  // ChromeViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  TestUpdatePropagator update_propagator_;
  std::unique_ptr<views::Widget> widget_;
  testing::NiceMock<MockWebUIToolbarControlDelegate> mock_toolbar_delegate_;
  std::unique_ptr<WebUIReadOnlyOmnibox> omnibox_view_;

  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> wc1_;
  raw_ptr<content::WebContents> wc2_;
};

void WebUIReadOnlyOmniboxTest::SetUp() {
  ChromeViewsTestBase::SetUp();

  profile_ = TestingProfile::Builder().Build();

  auto omnibox_client = std::make_unique<TestOmniboxClient>();
  omnibox_client_ = omnibox_client.get();
  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::move(omnibox_client));

  EXPECT_CALL(*omnibox_client_, GetPrefs())
      .WillRepeatedly(testing::Return(profile_->GetPrefs()));

  omnibox::RegisterProfilePrefs(
      static_cast<sync_preferences::TestingPrefServiceSyncable*>(
          omnibox_controller_->autocomplete_controller()
              ->autocomplete_provider_client()
              ->GetPrefs())
          ->registry());

  widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto contents_view = std::make_unique<views::View>();
  contents_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget_->SetContentsView(std::move(contents_view));
  widget_->Show();
  widget_->Activate();

  ON_CALL(mock_toolbar_delegate_, GetView())
      .WillByDefault(testing::Return(widget_->GetContentsView()));
  ON_CALL(mock_toolbar_delegate_, GetInternalWebView())
      .WillByDefault(testing::Return(widget_->GetContentsView()));
  // Mirrors WebUIToolbarWebView::OnFocusRequested(), which focuses the
  // toolbar's WebView in addition to forwarding the request to the WebUI.
  ON_CALL(mock_toolbar_delegate_, OnFocusRequested(testing::_))
      .WillByDefault([this](toolbar_ui_api::mojom::FocusRequestTarget) {
        widget_->GetContentsView()->RequestFocus();
      });
  update_propagator_.set_toolbar_delegate(&mock_toolbar_delegate_);

  omnibox_view_ = std::make_unique<WebUIReadOnlyOmnibox>(
      /*location_bar=*/nullptr, &mock_toolbar_delegate_,
      omnibox_controller_.get(), update_propagator_);

  wc1_ = web_contents_factory_.CreateWebContents(profile_.get());
  wc2_ = web_contents_factory_.CreateWebContents(profile_.get());
}

void WebUIReadOnlyOmniboxTest::TearDown() {
  web_contents_factory_.DestroyWebContents(wc1_.ExtractAsDangling());
  web_contents_factory_.DestroyWebContents(wc2_.ExtractAsDangling());
  widget_.reset();
  ChromeViewsTestBase::TearDown();
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
                  /*browser_version=*/1, /*ui_version=*/10, /*unelision=*/false,
                  /*paste=*/false, gfx::Range(18))))
          .has_value());

  // State will reflect it, including the version.
  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  // Views omnibox would highlight in this case, but we can't render that
  // when editable anyway, so might as well not spend the cycles.
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("https://en.wikiped",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(10u, mojo_state->ui_version);

  // Resetting the URL should bump the browser version and send the new URL.
  omnibox_view_->RevertAll();
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("www.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_EQ(2u, mojo_state->browser_version);
  EXPECT_EQ(0u, mojo_state->ui_version);

  // Racing input gets ignored.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://en.wikipedi", /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/11, /*unelision=*/false,
                  /*paste=*/false, gfx::Range(19))))
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
  EXPECT_EQ(2u, mojo_state->browser_version);
  EXPECT_EQ(0u, mojo_state->ui_version);

  // Now an update with appropriate browser version will work.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://www.example.org/a",
                  /*inline_completion=*/u"",
                  /*browser_version=*/2, /*ui_version=*/1, /*unelision=*/false,
                  /*paste=*/false, gfx::Range(25))))
          .has_value());
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  // Views omnibox would highlight in this case, but we can't render that
  // when editable anyway, so might as well not spend the cycles.
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("https://www.example.org/a",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_EQ(2u, mojo_state->browser_version);
  EXPECT_EQ(1u, mojo_state->ui_version);
}

TEST_F(WebUIReadOnlyOmniboxTest, ClearInputFromWebUI) {
  std::u16string initial_text = u"https://www.example.org/";
  location_bar_model()->set_url(GURL(initial_text));
  omnibox_view_->Update();

  // Verify initial state.
  EXPECT_EQ(initial_text, omnibox_view_->GetText());

  // Simulate WebUI clearing the text.
  // This sends an empty text with updated UI version.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"", /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/10, /*unelision=*/false,
                  /*paste=*/false, gfx::Range(0))))
          .has_value());

  // State will reflect it.
  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(mojo_state->text_pieces, ElementsAre());  // Empty text pieces
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(10u, mojo_state->ui_version);
  EXPECT_EQ(u"", omnibox_view_->GetText());
}

TEST_F(WebUIReadOnlyOmniboxTest, UnelideUserInputBit) {
  location_bar_model()->set_url(GURL("https://www.example.org/"));
  location_bar_model()->set_url_for_display(u"www.example.org");
  omnibox_view_->Update();

  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("www.example.org", OmniboxTextColor::kOmniboxText)));
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(0u, mojo_state->ui_version);

  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://www.example.org/",
                  /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/1, /*unelision=*/true,
                  /*paste=*/false, gfx::Range(6))))
          .has_value());
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_THAT(
      mojo_state->text_pieces,
      ElementsAre(IsSpan("https://", OmniboxTextColor::kOmniboxTextDimmed),
                  IsSpan("www.example.org", OmniboxTextColor::kOmniboxText),
                  IsSpan("/", OmniboxTextColor::kOmniboxTextDimmed)));
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(1u, mojo_state->ui_version);

  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://awww.example.org/",
                  /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/2, /*unelision=*/false,
                  /*paste=*/false, gfx::Range(7))))
          .has_value());
  mojo_state = update_propagator_.TakeState();
  EXPECT_THAT(mojo_state->text_pieces,
              ElementsAre(IsSpan("https://awww.example.org/",
                                 OmniboxTextColor::kOmniboxText)));
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(2u, mojo_state->ui_version);
}

TEST_F(WebUIReadOnlyOmniboxTest, ContextualTasksFocusBlur) {
  // Set up contextual tasks page.
  location_bar_model()->set_is_contextual_tasks_page(true);
  std::u16string display_url = u"chrome://google.com/search?q=test";
  location_bar_model()->set_url_for_display(display_url);
  omnibox_view_->Update();  // Pull initial state

  // Initially not focused, should show display URL, but input NOT in progress.
  toolbar_ui_api::mojom::OmniboxViewStatePtr mojo_state;
  EXPECT_EQ(display_url, omnibox_view_->GetText());
  {
    mojo_state = update_propagator_.TakeState();
    ASSERT_TRUE(mojo_state);
    EXPECT_FALSE(mojo_state->user_input_in_progress);
  }

  // Focus the omnibox.
  FocusOmnibox(mojo_state->browser_version);

  // Should still show display URL, and user input is NOT in progress.
  EXPECT_EQ(display_url, omnibox_view_->GetText());
  {
    mojo_state = update_propagator_.TakeState();
    ASSERT_TRUE(mojo_state);
    EXPECT_FALSE(mojo_state->user_input_in_progress);
  }

  // Blur the omnibox.
  BlurOmnibox(mojo_state->browser_version);

  // Should still show display URL, and user input is NOT in progress again.
  EXPECT_EQ(display_url, omnibox_view_->GetText());
  {
    mojo_state = update_propagator_.TakeState();
    ASSERT_TRUE(mojo_state);
    EXPECT_FALSE(mojo_state->user_input_in_progress);
  }
}

TEST_F(WebUIReadOnlyOmniboxTest, OnPointer) {
  // Sending pointer down action should succeed.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewPointer(
              toolbar_ui_api::mojom::OmniboxActionPointer::New(
                  /*is_pointer_down=*/true, /*start_zero_suggest=*/false,
                  gfx::Range(0))))
          .has_value());

  // Sending pointer up action with start_zero_suggest=true should succeed.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewPointer(
              toolbar_ui_api::mojom::OmniboxActionPointer::New(
                  /*is_pointer_down=*/false, /*start_zero_suggest=*/true,
                  gfx::Range(0))))
          .has_value());
}

TEST_F(WebUIReadOnlyOmniboxTest, SetUserTextBumpsBrowserVersion) {
  location_bar_model()->set_url(GURL("https://www.example.org/"));
  location_bar_model()->set_url_for_display(u"www.example.org");
  omnibox_view_->Update();

  auto mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_EQ(1u, mojo_state->browser_version);
  EXPECT_EQ(0u, mojo_state->ui_version);

  // Setting user text from the browser should bump browser_version and reset
  // ui_version.
  omnibox_view_->SetUserText(u"Typing in the Omnibox...");
  mojo_state = update_propagator_.TakeState();
  ASSERT_TRUE(mojo_state);
  EXPECT_EQ(2u, mojo_state->browser_version);
  EXPECT_EQ(0u, mojo_state->ui_version);
  EXPECT_EQ(u"Typing in the Omnibox...", omnibox_view_->GetText());

  // A racing unelision input with stale browser_version (e.g. triggered by a
  // focus request in flight before SetUserText) should be ignored.
  EXPECT_TRUE(
      omnibox_view_
          ->OnOmniboxAction(toolbar_ui_api::mojom::OmniboxAction::NewTextInput(
              toolbar_ui_api::mojom::OmniboxActionTextInput::New(
                  /*text=*/u"https://www.example.org/",
                  /*inline_completion=*/u"",
                  /*browser_version=*/1, /*ui_version=*/1, /*unelision=*/true,
                  /*paste=*/false, gfx::Range(0, 24))))
          .has_value());
  EXPECT_FALSE(update_propagator_.TakeState());
  EXPECT_EQ(u"Typing in the Omnibox...", omnibox_view_->GetText());
}

TEST_F(WebUIReadOnlyOmniboxTest, RevertOnBlur) {
  location_bar_model()->set_url(GURL("https://example.com/"));
  location_bar_model()->set_url_for_display(u"example.com");
  omnibox_view_->Update();

  EXPECT_EQ(u"example.com", omnibox_view_->GetText());
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());

  FocusOmnibox();

  // Unelide the URL. This changes the view text to the full URL without
  // putting the model into user input mode.
  EXPECT_TRUE(omnibox_controller_->edit_model()->Unelide());
  EXPECT_EQ(u"https://example.com/", omnibox_view_->GetText());
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());

  // Expect that on blur, we revert to the elided display text.
  BlurOmnibox();
  EXPECT_EQ(u"example.com", omnibox_view_->GetText());
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());

  // Now focus and set user text that matches permanent display text.
  FocusOmnibox();
  omnibox_view_->SetUserText(u"example.com");
  EXPECT_EQ(u"example.com", omnibox_view_->GetText());
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());

  // On blur, since text matches permanent text, user input mode is reset via
  // RevertAll().
  BlurOmnibox();
  EXPECT_EQ(u"example.com", omnibox_view_->GetText());
  EXPECT_FALSE(omnibox_controller_->edit_model()->user_input_in_progress());
}

TEST_F(WebUIReadOnlyOmniboxTest, OnBlurPreservesSelection) {
  location_bar_model()->set_url(GURL("https://example.com/"));
  location_bar_model()->set_url_for_display(u"example.com");
  omnibox_view_->Update();

  FocusOmnibox();

  std::u16string url = u"about:blank";
  omnibox_view_->SetUserText(url);
  omnibox_view_->SelectAll(/*reversed=*/false);
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(gfx::Range(0, url.size()), omnibox_view_->GetSelectionBounds());
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());

  // Blur the omnibox. Since user input is in progress and differs from
  // permanent text, RevertAll() is not called and selection is preserved.
  BlurOmnibox();
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(gfx::Range(0, url.size()), omnibox_view_->GetSelectionBounds());
  EXPECT_EQ(url, omnibox_view_->GetText());
  EXPECT_TRUE(omnibox_controller_->edit_model()->user_input_in_progress());
}

TEST_F(WebUIReadOnlyOmniboxTest, SaveStateToTabFocusState) {
  // If focus state is OMNIBOX_FOCUS_INVISIBLE (e.g. fakebox focus on NTP),
  // saving state to tab should preserve OMNIBOX_FOCUS_INVISIBLE.
  omnibox_controller_->edit_model()->OnSetFocus(/*control_down=*/false);
  omnibox_controller_->edit_model()->SetCaretVisibility(false);
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE,
            omnibox_controller_->edit_model()->focus_state());

  omnibox_view_->SaveStateToTab(wc1_);

  const OmniboxState* state1 = static_cast<OmniboxState*>(
      wc1_->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  ASSERT_TRUE(state1);
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE, state1->model_state.focus_state);

  // If focus state is OMNIBOX_FOCUS_VISIBLE, saving state should preserve it.
  omnibox_controller_->edit_model()->SetCaretVisibility(true);
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE,
            omnibox_controller_->edit_model()->focus_state());

  omnibox_view_->SaveStateToTab(wc2_);

  const OmniboxState* state2 = static_cast<OmniboxState*>(
      wc2_->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  ASSERT_TRUE(state2);
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE, state2->model_state.focus_state);
}

TEST_F(WebUIReadOnlyOmniboxTest, SetFocusUpdatesEditModelSynchronously) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::internal::kWebUIOmniboxFullPopup);

  EXPECT_FALSE(omnibox_controller_->edit_model()->has_focus());

  // If the focus request can't land on the web view (e.g. it isn't focusable
  // because the toolbar is hidden), SetFocus() must not synchronously mark the
  // edit model focused.
  views::View* web_view = widget_->GetContentsView();
  web_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  omnibox_view_->SetFocus(/*is_user_initiated=*/false);
  EXPECT_FALSE(web_view->HasFocus());
  EXPECT_FALSE(omnibox_controller_->edit_model()->has_focus());
  web_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  // Normally, SetFocus() synchronously focuses the web view via
  // PropagateFocusRequest() and should update the edit model's focus state
  // before the asynchronous Mojo OnFocusChange notification arrives (e.g.
  // during tab switch focus restoration).
  omnibox_view_->SetFocus(/*is_user_initiated=*/false);
  EXPECT_TRUE(web_view->HasFocus());
  EXPECT_TRUE(omnibox_controller_->edit_model()->has_focus());

  // When the WebUI subsequently reports OnFocusChange(has_focus=true), the edit
  // model remains focused without redundant focus state transitions.
  EXPECT_TRUE(omnibox_view_
                  ->OnOmniboxAction(
                      toolbar_ui_api::mojom::OmniboxAction::NewFocusChange(
                          toolbar_ui_api::mojom::OmniboxActionFocusChange::New(
                              /*has_focus=*/true,
                              /*request_clear_keyword=*/false,
                              /*activate_default_search=*/false,
                              /*start_zero_suggest=*/false,
                              /*browser_version=*/0,
                              /*selection=*/gfx::Range(0))))
                  .has_value());
  EXPECT_TRUE(omnibox_controller_->edit_model()->has_focus());

  // Blurring resets the focus state.
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_TRUE(omnibox_view_
                  ->OnOmniboxAction(
                      toolbar_ui_api::mojom::OmniboxAction::NewFocusChange(
                          toolbar_ui_api::mojom::OmniboxActionFocusChange::New(
                              /*has_focus=*/false,
                              /*request_clear_keyword=*/false,
                              /*activate_default_search=*/false,
                              /*start_zero_suggest=*/false,
                              /*browser_version=*/0,
                              /*selection=*/gfx::Range(0))))
                  .has_value());
  EXPECT_FALSE(omnibox_controller_->edit_model()->has_focus());
}

TEST_F(WebUIReadOnlyOmniboxTest, SetFocusRestoresCaretVisibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::internal::kWebUIOmniboxFullPopup);

  // Ensure the internal web view is not focused so OnTabChanged() doesn't
  // itself request focus.
  widget_->GetFocusManager()->ClearFocus();

  // Save invisible focus state (e.g. fakebox focus on NTP) to wc1_.
  omnibox_controller_->edit_model()->OnSetFocus(/*control_down=*/false);
  omnibox_controller_->edit_model()->SetCaretVisibility(false);
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE,
            omnibox_controller_->edit_model()->focus_state());
  omnibox_view_->SaveStateToTab(wc1_);

  // Simulate a tab switch away so the current focus state is cleared.
  omnibox_controller_->edit_model()->OnKillFocus();
  EXPECT_EQ(OMNIBOX_FOCUS_NONE,
            omnibox_controller_->edit_model()->focus_state());

  // Tab switch to a tab with invisible focus.
  omnibox_view_->OnTabChanged(wc1_);
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE,
            omnibox_controller_->edit_model()->focus_state());

  // Explicitly requesting focus restores caret visibility.
  omnibox_view_->SetFocus(/*is_user_initiated=*/false);
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE,
            omnibox_controller_->edit_model()->focus_state());
}

}  // namespace
