// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

class TestLocationBarViewDelegate : public LocationBarView::Delegate {
 public:
  explicit TestLocationBarViewDelegate(LocationBarModel* model)
      : model_(model) {}

  content::WebContents* GetWebContents() override { return nullptr; }
  LocationBarModel* GetLocationBarModel() override { return model_; }
  const LocationBarModel* GetLocationBarModel() const override {
    return model_;
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return nullptr;
  }

 private:
  raw_ptr<LocationBarModel> model_;
};

}  // namespace

class WebUILocationBarTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(mock_browser_, GetBrowserForMigrationOnly())
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(mock_browser_, GetProfile())
        .WillByDefault(testing::Return(&profile_));
    ON_CALL(mock_browser_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    location_bar_model_ = std::make_unique<TestLocationBarModel>();
    delegate_ = std::make_unique<TestLocationBarViewDelegate>(
        location_bar_model_.get());

    auto location_bar =
        std::make_unique<WebUILocationBar>(nullptr, delegate_.get());
    location_bar_ = location_bar.get();

    toolbar_view_ = std::make_unique<WebUIToolbarWebView>(
        &mock_browser_, nullptr, std::move(location_bar));

    // Assign directly instead of calling Init() to avoid initializing heavy
    // web UI popups that require fully attached widgets and profiles.
    location_bar_->toolbar_delegate_ = toolbar_view_.get();

    fetcher_ = toolbar_view_->GetNavigationControlsStateFetcher();
  }

  toolbar_ui_api::mojom::NavigationControlsStatePtr GetState() {
    return fetcher_->GetNavigationControlsState();
  }

  bool GetSuppressLhsChipClicked() const {
    return location_bar_->suppress_lhs_chip_clicked_;
  }

  void SimulatePageInfoBubbleClosed() {
    location_bar_->OnPageInfoBubbleClosed(
        views::Widget::ClosedReason::kCloseButtonClicked, false);
  }

  WebUIPermissionDashboard* permission_dashboard() {
    return location_bar_->permission_dashboard_.get();
  }

  content::BrowserTaskEnvironment browser_threads_;
  TestingProfile profile_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationBarViewDelegate> delegate_;
  std::unique_ptr<WebUIToolbarWebView> toolbar_view_;
  raw_ptr<WebUILocationBar> location_bar_ = nullptr;
  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher> fetcher_;
};

TEST_F(WebUILocationBarTest, StateManagement_SecurityChip) {
  const struct {
    std::string_view name;
    security_state::SecurityLevel security_level;
    std::u16string display_text;
    toolbar_ui_api::mojom::SecurityChipIcon expected_icon;
    toolbar_ui_api::mojom::SecurityLevel expected_mojo_level;
  } kTestCases[] = {
      {"Secure", security_state::SECURE, std::u16string(),
       toolbar_ui_api::mojom::SecurityChipIcon::kSecurePageInfo,
       toolbar_ui_api::mojom::SecurityLevel::kSecure},
      {"Dangerous", security_state::DANGEROUS, u"Dangerous",
       toolbar_ui_api::mojom::SecurityChipIcon::kDangerous,
       toolbar_ui_api::mojom::SecurityLevel::kDangerous},
      {"Warning", security_state::WARNING, u"Not secure",
       toolbar_ui_api::mojom::SecurityChipIcon::kNotSecureWarning,
       toolbar_ui_api::mojom::SecurityLevel::kWarning},
      {"None", security_state::NONE, std::u16string(),
       toolbar_ui_api::mojom::SecurityChipIcon::kHttp,
       toolbar_ui_api::mojom::SecurityLevel::kNone},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    location_bar_model_->set_security_level(test_case.security_level);
    location_bar_model_->set_secure_display_text(test_case.display_text);

    // Force update
    location_bar_->OnChanged();

    auto state = GetState();
    ASSERT_TRUE(state);
    ASSERT_TRUE(state->location_bar_state);
    ASSERT_TRUE(state->location_bar_state->lhs_chips_state);
    ASSERT_TRUE(state->location_bar_state->lhs_chips_state->security_chip);

    const auto& chip =
        state->location_bar_state->lhs_chips_state->security_chip;
    EXPECT_EQ(chip->icon, test_case.expected_icon);
    EXPECT_EQ(chip->security_level, test_case.expected_mojo_level);
    EXPECT_EQ(chip->text, test_case.display_text);
    EXPECT_TRUE(chip->is_clickable);
  }
}

TEST_F(WebUILocationBarTest, StateManagement_PermissionChip) {
  WebUIPermissionChip chip(location_bar_);

  chip.SetVisible(true);
  chip.SetChipIcon(kCameraIcon);
  chip.SetMessage(u"Camera in use");
  chip.SetTooltipText(u"Tooltip");
  chip.SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  chip.SetUserDecision(permissions::PermissionAction::GRANTED);
  chip.SetBlockedIconShowing(false);
  chip.SetPermissionPromptStyle(PermissionPromptStyle::kChip);
  chip.SetAccessibilityName(u"Camera");
  chip.AnimateExpand(base::Milliseconds(100));

  auto state = chip.GetState();
  EXPECT_TRUE(state->is_visible);
  EXPECT_EQ(state->icon_name, "kCameraIcon");
  EXPECT_EQ(state->message, u"Camera in use");
  EXPECT_EQ(state->tooltip, u"Tooltip");
  EXPECT_EQ(
      state->theme,
      toolbar_ui_api::mojom::PermissionChipTheme::kInUseActivityIndicator);
  EXPECT_EQ(state->user_decision,
            toolbar_ui_api::mojom::PermissionAction::kGranted);
  EXPECT_FALSE(state->should_show_blocked_icon);
  EXPECT_EQ(state->prompt_style,
            toolbar_ui_api::mojom::PermissionPromptStyle::kChip);
  EXPECT_FALSE(state->is_fully_collapsed);
  EXPECT_EQ(state->accessibility_name, u"Camera");
}

TEST_F(WebUILocationBarTest, HasSecurityStateChanged) {
  // Start with SECURE state.
  location_bar_model_->set_security_level(security_state::SECURE);
  location_bar_->OnChanged();
  EXPECT_FALSE(location_bar_->HasSecurityStateChanged());

  // Mutate the model directly without telling the location bar.
  location_bar_model_->set_security_level(security_state::DANGEROUS);

  // The location bar should detect that its internal state is now out of sync.
  EXPECT_TRUE(location_bar_->HasSecurityStateChanged());

  // Force an update to sync them back up.
  location_bar_->OnChanged();
  EXPECT_FALSE(location_bar_->HasSecurityStateChanged());
}

TEST_F(WebUILocationBarTest, MouseClickSuppression) {
  // By default, suppression is false.
  EXPECT_FALSE(GetSuppressLhsChipClicked());

  // A mouse press on the chip should NOT suppress if the bubble wasn't just
  // closed.
  location_bar_->OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon);
  EXPECT_FALSE(GetSuppressLhsChipClicked());

  // Simulate the bubble being closed right now.
  SimulatePageInfoBubbleClosed();

  // A mouse press immediately after closing should trigger suppression.
  location_bar_->OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon);
  EXPECT_TRUE(GetSuppressLhsChipClicked());

  // A non-mouse click (e.g., keyboard Enter) should NOT consume the suppression
  // flag.
  location_bar_->OnLhsChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon,
      /*is_mouse_interaction=*/false);
  EXPECT_TRUE(GetSuppressLhsChipClicked());

  // A true mouse click SHOULD consume the suppression flag and return early.
  location_bar_->OnLhsChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon,
      /*is_mouse_interaction=*/true);
  EXPECT_FALSE(GetSuppressLhsChipClicked());
}
