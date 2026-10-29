// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/icu_test_util.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_delegate.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_highlighter_views.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_utils.h"

namespace bubble_anchor_util {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSeparatorElementId);

class BubbleAnchorUtilViewTest : public ChromeViewsTestBase {
 public:
  BubbleAnchorUtilViewTest() = default;

  BubbleAnchorUtilViewTest(const BubbleAnchorUtilViewTest&) = delete;
  BubbleAnchorUtilViewTest& operator=(const BubbleAnchorUtilViewTest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ui::ElementHighlighter::GetElementHighlighter()
        ->MaybeRegisterBackend<views::ElementHighlighterViews>();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    // Create a button (higlightable) and separator (not), and give them IDs.
    button_ = widget_->SetContentsView(std::make_unique<views::LabelButton>(
        views::Button::PressedCallback(), std::u16string()));
    button_->SetProperty(views::kElementIdentifierKey, kButtonElementId);
    button_->SetVisible(true);
    separator_ = button_->AddChildView(std::make_unique<views::Separator>());
    separator_->SetProperty(views::kElementIdentifierKey, kSeparatorElementId);
    separator_->SetVisible(true);
  }

  void TearDown() override {
    button_ = nullptr;
    separator_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::LabelButton> button_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;
};

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableView) {
  EXPECT_TRUE(IsHighlightable(views::BubbleAnchor(button_)));
  EXPECT_FALSE(IsHighlightable(views::BubbleAnchor(separator_)));
}

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableElement) {
  ui::TrackedElement* button_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(button_);
  ASSERT_TRUE(button_element);
  ui::TrackedElement* separator_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(separator_);
  ASSERT_TRUE(separator_element);

  EXPECT_TRUE(IsHighlightable(views::BubbleAnchor(button_element)));
  EXPECT_FALSE(IsHighlightable(views::BubbleAnchor(separator_element)));
}

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableNull) {
  EXPECT_FALSE(IsHighlightable(views::BubbleAnchor()));
}

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverrideAnchorElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverrideChipElementId);

// A `LocationBar` standing in for a secondary surface's location bar (e.g. the
// Contextual Tasks side panel toolbar) attached via `LocationBarOverrideData`.
class FakeOverrideLocationBar : public LocationBar {
 public:
  FakeOverrideLocationBar() : LocationBar(nullptr) {}
  ~FakeOverrideLocationBar() override = default;

  void set_anchor_element(ui::TrackedElement* anchor_element) {
    anchor_element_ = anchor_element;
  }
  void set_chip_anchor(std::optional<AnchorConfiguration> chip_anchor) {
    chip_anchor_ = std::move(chip_anchor);
  }
  void set_is_drawn(bool is_drawn) { is_drawn_ = is_drawn; }
  void set_chip_controller(ChipController* chip_controller) {
    chip_controller_ = chip_controller;
  }

  // LocationBar:
  void FocusLocation(bool is_user_initiated,
                     bool clear_focus_if_failed) override {}
  void FocusSearch() override {}
  void UpdateFocusBehavior(bool toolbar_visible) override {}
  void UpdateContentSettingsIcons() override {}
  void SaveStateToContents(content::WebContents* contents) override {}
  void Revert() override {}
  OmniboxView* GetOmniboxView() override { return nullptr; }
  OmniboxPopupView* GetOmniboxPopupView() override { return nullptr; }
  OmniboxController* GetOmniboxController() override { return nullptr; }
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override { return false; }
  content::WebContents* GetWebContents() override { return nullptr; }
  LocationBarModel* GetLocationBarModel() override { return nullptr; }
  std::optional<AnchorConfiguration> GetChipAnchor() override {
    return chip_anchor_;
  }
  ChipController* GetChipController() override { return chip_controller_; }
  void AnnounceAlert(const std::u16string& announcement) override {}
  void OnChanged() override {}
  void UpdateWithoutTabRestore() override {}
  ui::TrackedElement* GetAnchorOrNull() override { return anchor_element_; }
  BrowserWindowInterface* GetBrowser() override { return nullptr; }
  Profile* GetProfile() override { return nullptr; }
  bool IsInitialized() const override { return true; }
  bool IsVisible() const override { return is_drawn_; }
  bool IsDrawn() const override { return is_drawn_; }
  bool IsFullscreen() const override { return false; }
  bool IsEditingOrEmpty() const override { return false; }
  bool IsMouseHovered() const override { return false; }
  bool IsFocusWithin() const override { return false; }
  void InvalidateLayout() override {}
  gfx::Rect Bounds() const override { return gfx::Rect(); }
  gfx::Rect BoundsInScreen() const override { return gfx::Rect(); }
  gfx::Size MinimumSize() const override { return gfx::Size(); }
  gfx::Size PreferredSize() const override { return gfx::Size(); }
  void Update(content::WebContents* contents) override {}
  void ResetTabState(content::WebContents* contents) override {}
  bool HasSecurityStateChanged() override { return false; }
  LocationBarTesting* GetLocationBarForTesting() override { return nullptr; }

 private:
  raw_ptr<ui::TrackedElement> anchor_element_ = nullptr;
  std::optional<AnchorConfiguration> chip_anchor_;
  raw_ptr<ChipController> chip_controller_ = nullptr;
  bool is_drawn_ = true;
};

}  // namespace

// Covers `GetPermissionPromptBubbleAnchorConfiguration()` for a `WebContents`
// that is not a tab and whose location bar is overridden, such as the
// Contextual Tasks side panel.
class BubbleAnchorUtilOverrideTest : public ChromeRenderViewHostTestHarness {
 protected:
  BubbleAnchorUtilOverrideTest() = default;
  ~BubbleAnchorUtilOverrideTest() override = default;

  ui::test::TestElement anchor_element_{
      kOverrideAnchorElementId,
      ui::ElementContext::CreateFakeContextForTesting(1)};
  ui::test::TestElement chip_element_{
      kOverrideChipElementId,
      ui::ElementContext::CreateFakeContextForTesting(1)};
};

TEST_F(BubbleAnchorUtilOverrideTest, AnchorsToOverrideLocationBar) {
  FakeOverrideLocationBar location_bar;
  location_bar.set_anchor_element(&anchor_element_);
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  AnchorConfiguration config =
      GetPermissionPromptBubbleAnchorConfiguration(web_contents());
  EXPECT_EQ(config.anchor.GetIfElement(), &anchor_element_);
  // Highlighting by element ID would resolve within the hosting window.
  EXPECT_FALSE(config.highlighted_element.has_value());
  EXPECT_EQ(config.bubble_arrow, views::BubbleBorder::TOP_LEFT);
}

// The chip anchor is only used while a permission prompt chip is showing.
// Without a `ChipController`, the location bar's general anchor is used.
TEST_F(BubbleAnchorUtilOverrideTest, IgnoresChipAnchorWithoutVisibleChip) {
  FakeOverrideLocationBar location_bar;
  location_bar.set_anchor_element(&anchor_element_);
  location_bar.set_chip_anchor(
      AnchorConfiguration{views::BubbleAnchor(&chip_element_), std::nullopt,
                          views::BubbleBorder::TOP_LEFT});
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  EXPECT_EQ(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                .anchor.GetIfElement(),
            &anchor_element_);
}

// With a real `ChipController`, the chip anchor is used only while its
// permission prompt chip is visible.
TEST_F(BubbleAnchorUtilOverrideTest, AnchorsToChipOnlyWhilePromptChipVisible) {
  permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kGeolocation));
  testing::NiceMock<permissions::MockPermissionPromptDelegate> delegate;
  ON_CALL(delegate, Requests()).WillByDefault(testing::ReturnRef(requests));
  ON_CALL(delegate, GetAssociatedWebContents())
      .WillByDefault(testing::Return(web_contents()));
  base::WeakPtrFactory<permissions::PermissionPrompt::Delegate>
      delegate_weak_factory(&delegate);

  FakeOverrideLocationBar location_bar;
  WebUIPermissionChip chip(&location_bar,
                           PermissionChipView::kPermissionRequestChipElementId);
  ChipController chip_controller(&location_bar,
                                 /*content_settings_image_delegate=*/nullptr,
                                 &chip);
  location_bar.set_chip_controller(&chip_controller);
  location_bar.set_anchor_element(&anchor_element_);
  location_bar.set_chip_anchor(
      AnchorConfiguration{views::BubbleAnchor(&chip_element_), std::nullopt,
                          views::BubbleBorder::TOP_LEFT});
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  // No prompt yet, so the chip isn't showing.
  ASSERT_FALSE(chip_controller.IsPermissionPromptChipVisible());
  EXPECT_EQ(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                .anchor.GetIfElement(),
            &anchor_element_);

  chip_controller.InitializePermissionPrompt(
      delegate_weak_factory.GetWeakPtr());
  chip.SetVisible(true);
  ASSERT_TRUE(chip_controller.IsPermissionPromptChipVisible());
  EXPECT_EQ(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                .anchor.GetIfElement(),
            &chip_element_);

  chip.SetVisible(false);
  ASSERT_FALSE(chip_controller.IsPermissionPromptChipVisible());
  EXPECT_EQ(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                .anchor.GetIfElement(),
            &anchor_element_);
}

// When the override yields no anchor, the fallback rect is at the top of the
// side panel `WebContents`.
TEST_F(BubbleAnchorUtilOverrideTest, AnchorRectIsRelativeToOverriddenContents) {
  FakeOverrideLocationBar location_bar;
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);
  ASSERT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());

  const gfx::Rect bounds = web_contents()->GetContainerBounds();
  EXPECT_EQ(GetPermissionPromptBubbleAnchorRect(web_contents()),
            gfx::Rect(bounds.x() + kNoToolbarLeftOffset, bounds.y(), 0, 0));
}

// In RTL, the fallback rect is inset from the right (leading) edge instead.
TEST_F(BubbleAnchorUtilOverrideTest, AnchorRectIsMirroredInRtl) {
  base::test::ScopedRestoreICUDefaultLocale rtl_locale("he");
  ASSERT_TRUE(base::i18n::IsRTL());

  FakeOverrideLocationBar location_bar;
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);
  ASSERT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());

  const gfx::Rect bounds = web_contents()->GetContainerBounds();
  EXPECT_EQ(GetPermissionPromptBubbleAnchorRect(web_contents()),
            gfx::Rect(bounds.right() - kNoToolbarLeftOffset, bounds.y(), 0, 0));
}

// A `WebContents` without an override that isn't hosted in a browser window
// has no window to position relative to, so the fallback rect is relative to
// the `WebContents` itself.
TEST_F(BubbleAnchorUtilOverrideTest,
       AnchorRectWithoutBrowserIsRelativeToContents) {
  ASSERT_FALSE(
      location_bar::LocationBarOverrideData::FromWebContents(web_contents()));
  ASSERT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());

  const gfx::Rect bounds = web_contents()->GetContainerBounds();
  EXPECT_EQ(GetPermissionPromptBubbleAnchorRect(web_contents()),
            gfx::Rect(bounds.x() + kNoToolbarLeftOffset, bounds.y(), 0, 0));
}

TEST_F(BubbleAnchorUtilOverrideTest, NotDrawnReturnsNullAnchor) {
  FakeOverrideLocationBar location_bar;
  location_bar.set_anchor_element(&anchor_element_);
  location_bar.set_is_drawn(false);
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  EXPECT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());
}

TEST_F(BubbleAnchorUtilOverrideTest, NoAnchorElementReturnsNullAnchor) {
  FakeOverrideLocationBar location_bar;
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  EXPECT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());
}

TEST_F(BubbleAnchorUtilOverrideTest, NullOverrideLocationBarReturnsNullAnchor) {
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              nullptr);

  EXPECT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());
}

TEST_F(BubbleAnchorUtilOverrideTest,
       DestroyedOverrideLocationBarReturnsNullAnchor) {
  auto location_bar = std::make_unique<FakeOverrideLocationBar>();
  location_bar->set_anchor_element(&anchor_element_);
  location_bar::LocationBarOverrideData::CreateForWebContents(
      web_contents(), location_bar.get());
  location_bar.reset();

  EXPECT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());
}

// Mirrors the side panel setup: the `WebContents` is embedded in a browser
// window via `webui::SetBrowserWindowInterface()`. The override must win, and
// the window (and thus its omnibox) must never be consulted.
TEST_F(BubbleAnchorUtilOverrideTest, OverrideTakesPrecedenceOverEmbedder) {
  testing::StrictMock<MockBrowserWindowInterface> embedding_browser;
  EXPECT_CALL(embedding_browser, RegisterBrowserDidClose)
      .WillOnce(
          testing::Return(testing::ByMove(base::CallbackListSubscription())));
  webui::SetBrowserWindowInterface(web_contents(), &embedding_browser);

  FakeOverrideLocationBar location_bar;
  location_bar.set_anchor_element(&anchor_element_);
  location_bar::LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                              &location_bar);

  EXPECT_EQ(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                .anchor.GetIfElement(),
            &anchor_element_);

  // Fail closed: with nothing to anchor to on the override surface, return a
  // null anchor rather than falling back to the embedding window, and position
  // the bubble relative to the side panel `WebContents`, not the window.
  location_bar.set_anchor_element(nullptr);
  EXPECT_TRUE(GetPermissionPromptBubbleAnchorConfiguration(web_contents())
                  .anchor.IsNull());
  const gfx::Rect bounds = web_contents()->GetContainerBounds();
  EXPECT_EQ(GetPermissionPromptBubbleAnchorRect(web_contents()),
            gfx::Rect(bounds.x() + kNoToolbarLeftOffset, bounds.y(), 0, 0));

  webui::SetBrowserWindowInterface(web_contents(), nullptr);
}

}  // namespace bubble_anchor_util
