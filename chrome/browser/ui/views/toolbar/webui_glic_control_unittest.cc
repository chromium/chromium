// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_control.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

auto GlicButtonStateMatches(bool open,
                            bool should_show,
                            bool is_context_menu_visible = false) {
  using toolbar_ui_api::mojom::GlicButtonState;
  return testing::Pointee(testing::AllOf(
      testing::Field("open", &GlicButtonState::open, open),
      testing::Field("should_show", &GlicButtonState::should_show, should_show),
      testing::Field("is_context_menu_visible",
                     &GlicButtonState::is_context_menu_visible,
                     is_context_menu_visible)));
}

class FakeThemeService : public ThemeService {
 public:
  explicit FakeThemeService(Profile* profile)
      : ThemeService(profile, GetFakeThemeHelper()) {}

  bool UsingExtensionTheme() const override {
    return is_using_extension_theme_;
  }
  void set_using_extension_theme(bool value) {
    is_using_extension_theme_ = value;
  }

 private:
  static const ThemeHelper& GetFakeThemeHelper() {
    static base::NoDestructor<ThemeHelper> helper;
    return *helper;
  }

  bool is_using_extension_theme_ = false;
};

}  // namespace

class WebUIGlicControlTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        ThemeServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakeThemeService>(
              Profile::FromBrowserContext(context));
        }));
    profile_ = profile_builder.Build();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::make_unique<views::View>());

    browser_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));

    delegate_ =
        std::make_unique<testing::NiceMock<MockWebUIToolbarControlDelegate>>();
    ON_CALL(*delegate_, GetBrowser())
        .WillByDefault(testing::Return(browser_interface_.get()));
    ON_CALL(*delegate_, GetView()).WillByDefault(testing::Return(view_.get()));

    control_ = std::make_unique<WebUIGlicControl>(delegate_.get());
    control_->Init();
  }

  void TearDown() override {
    control_.reset();
    delegate_.reset();
    browser_interface_.reset();
    view_ = nullptr;
    widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> view_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_interface_;
  std::unique_ptr<testing::NiceMock<MockWebUIToolbarControlDelegate>> delegate_;
  std::unique_ptr<WebUIGlicControl> control_;

  FakeThemeService* theme_service() {
    return static_cast<FakeThemeService*>(
        ThemeServiceFactory::GetForProfile(profile_.get()));
  }
};

TEST_F(WebUIGlicControlTest, PrefChangeNotifiesDelegate) {
  control_->SetVisible(true);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false)));
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/true)));
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
}

TEST_F(WebUIGlicControlTest, SetVisibleNotifiesDelegate) {
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/true)));
  control_->SetVisible(true);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false)));
  control_->SetVisible(false);
}

TEST_F(WebUIGlicControlTest, SetVisibleDoesNotOverwriteUnpinnedPref) {
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false)));
  control_->SetVisible(true);

  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
  EXPECT_FALSE(control_->IsVisible());
}

TEST_F(WebUIGlicControlTest, SetGlicPanelIsOpenNotifiesDelegate) {
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/true, /*should_show=*/false)));
  control_->SetGlicPanelIsOpen(true);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false)));
  control_->SetGlicPanelIsOpen(false);
}

TEST_F(WebUIGlicControlTest, ExecuteCommandUnpins) {
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  control_->ExecuteCommand(IDC_GLIC_TOGGLE_PIN, 0);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
}

TEST_F(WebUIGlicControlTest, HandleContextMenuDoesNotShowWhenUnpinned) {
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);

  control_->HandleContextMenu(gfx::Rect(), ui::mojom::MenuSourceType::kMouse);
  EXPECT_FALSE(control_->IsContextMenuShowingForTesting());
}

TEST_F(WebUIGlicControlTest,
       HandleContextMenuShowsAndUpdatesContextMenuVisibleState) {
  profile_->GetPrefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);

  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false,
                              /*is_context_menu_visible=*/true)));
  control_->HandleContextMenu(gfx::Rect(), ui::mojom::MenuSourceType::kMouse);
  EXPECT_TRUE(control_->IsContextMenuShowingForTesting());

  ASSERT_TRUE(views::MenuController::GetActiveInstance());
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged(GlicButtonStateMatches(
                              /*open=*/false, /*should_show=*/false,
                              /*is_context_menu_visible=*/false)));
  views::MenuController::GetActiveInstance()->Cancel(
      views::MenuController::ExitType::kAll);
  EXPECT_FALSE(control_->IsContextMenuShowingForTesting());
}

TEST_F(WebUIGlicControlTest, NudgeStateTrackedAndClearedOnClick) {
  EXPECT_FALSE(control_->GetIsShowingNudge());

  control_->SetNudgeLabel("Summarize this page");
  EXPECT_CALL(*delegate_,
              OnGlicButtonStateChanged(testing::Pointee(testing::Field(
                  &toolbar_ui_api::mojom::GlicButtonState::nudge_label,
                  std::optional<std::string>("Summarize this page")))));
  control_->SetIsShowingNudge(true);
  EXPECT_TRUE(control_->GetIsShowingNudge());

  EXPECT_CALL(*delegate_,
              OnGlicButtonStateChanged(testing::Pointee(testing::Field(
                  &toolbar_ui_api::mojom::GlicButtonState::nudge_label,
                  std::optional<std::string>(std::nullopt)))));
  control_->OnClicked();
  EXPECT_FALSE(control_->GetIsShowingNudge());
}

TEST_F(WebUIGlicControlTest, UpdateStyleNotifiesDelegate) {
  EXPECT_CALL(
      *delegate_,
      OnGlicButtonStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::GlicButtonState::should_match_toolbar_style,
          true))));
  control_->UpdateStyle(true);

  // Repeating the same style is a no-op.
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged).Times(0);
  control_->UpdateStyle(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(
      *delegate_,
      OnGlicButtonStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::GlicButtonState::should_match_toolbar_style,
          false))));
  control_->UpdateStyle(false);
}

TEST_F(WebUIGlicControlTest, InitSetsCustomThemeFallback) {
  theme_service()->set_using_extension_theme(true);
  auto control = std::make_unique<WebUIGlicControl>(delegate_.get());

  // OnThemeChanged() before Init() is a no-op.
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged).Times(0);
  control->OnThemeChanged();
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(
      *delegate_,
      OnGlicButtonStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::GlicButtonState::use_custom_theme_fallback,
          true))));
  control->Init();
}

TEST_F(WebUIGlicControlTest, OnThemeChangedUpdatesCustomThemeFallback) {
  EXPECT_CALL(
      *delegate_,
      OnGlicButtonStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::GlicButtonState::use_custom_theme_fallback,
          true))));
  theme_service()->set_using_extension_theme(true);
  control_->OnThemeChanged();

  // A theme change that doesn't affect the fallback is a no-op.
  EXPECT_CALL(*delegate_, OnGlicButtonStateChanged).Times(0);
  control_->OnThemeChanged();
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(
      *delegate_,
      OnGlicButtonStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::GlicButtonState::use_custom_theme_fallback,
          false))));
  theme_service()->set_using_extension_theme(false);
  control_->OnThemeChanged();
}
