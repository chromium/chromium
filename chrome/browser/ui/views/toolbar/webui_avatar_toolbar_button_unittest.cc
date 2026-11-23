// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;

class TestBrowserElements : public BrowserElements {
 public:
  DECLARE_SAFE_CAST_TARGET()

  TestBrowserElements(BrowserWindowInterface& browser,
                      ui::ElementContext context)
      : BrowserElements(browser), context_(context) {}
  ~TestBrowserElements() override = default;

  ui::ElementContext GetContext() override { return context_; }

 private:
  const ui::ElementContext context_;
};

DEFINE_SAFE_CAST_TARGET(TestBrowserElements)

class MockAvatarObserver : public AvatarToolbarButtonInterface::Observer {
 public:
  MOCK_METHOD(void, OnMouseExited, (), (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void, OnButtonPressed, (), (override));
  MOCK_METHOD(void, OnIconUpdated, (), (override));
  MOCK_METHOD(void, OnIPHPromoChanged, (bool has_promo), (override));
};

struct TestBrowserEnvironment {
  TestBrowserEnvironment(Profile* profile,
                         views::Widget* widget,
                         views::View* view)
      : user_education(&browser),
        elements(browser,
                 views::ElementTrackerViews::GetContextForWidget(widget)) {
    ON_CALL(browser, GetProfile()).WillByDefault(Return(profile));
    ON_CALL(std::as_const(browser), GetProfile())
        .WillByDefault(Return(profile));
    ON_CALL(delegate, GetBrowser()).WillByDefault(Return(&browser));
    ON_CALL(delegate, GetView()).WillByDefault(Return(view));
    ON_CALL(delegate, GetState()).WillByDefault(ReturnRef(toolbar_state));
  }

  NiceMock<MockBrowserWindowInterface> browser;
  NiceMock<MockBrowserUserEducationInterface> user_education;
  TestBrowserElements elements;
  toolbar_ui_api::mojom::ToolbarState toolbar_state;
  NiceMock<MockWebUIToolbarControlDelegate> delegate;
};

}  // namespace

class WebUIAvatarToolbarButtonTest : public ChromeViewsTestBase {
 public:
  WebUIAvatarToolbarButtonTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIAvatarButton}, {});
  }
  ~WebUIAvatarToolbarButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::make_unique<views::View>());
    view_->SetProperty(views::kElementIdentifierKey,
                       kToolbarAvatarButtonElementId);
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* CreateTestingProfile(const std::string& profile_name) {
    return profile_manager_.CreateTestingProfile(profile_name);
  }

  Profile* CreateEnabledProfile() {
    TestingProfile* normal_profile = CreateTestingProfile("enabled_base");
#if BUILDFLAG(IS_CHROMEOS)
    return TestingProfile::Builder().BuildIncognito(normal_profile);
#else
    return normal_profile;
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> view_ = nullptr;
};

TEST_F(WebUIAvatarToolbarButtonTest, ButtonPressedExecutesBeforeInitialize) {
  Profile* profile = CreateEnabledProfile();
  TestBrowserEnvironment env(profile, widget_.get(), view_);
  WebUIAvatarToolbarButton button(&env.delegate);

  NiceMock<MockAvatarObserver> observer;
  button.AddObserver(&observer);

  EXPECT_TRUE(button.IsButtonEnabled());
  EXPECT_CALL(observer, OnButtonPressed()).Times(2);
  button.ButtonPressed(/*is_source_accelerator=*/false);
  button.ButtonPressed(/*is_source_accelerator=*/true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(env.delegate,
              OnAvatarControlStateChanged(Pointee(Field(
                  &toolbar_ui_api::mojom::AvatarControlState::enabled, true))));
  button.Initialize();
  testing::Mock::VerifyAndClearExpectations(&env.delegate);
  EXPECT_TRUE(button.IsButtonEnabled());

  EXPECT_CALL(observer, OnButtonPressed()).Times(1);
  button.ButtonPressed(/*is_source_accelerator=*/false);

  button.RemoveObserver(&observer);
}

TEST_F(WebUIAvatarToolbarButtonTest, DisabledWhenCannotShowForProfile) {
  TestingProfile* normal_profile = CreateTestingProfile("normal");
  TestingProfile::Builder devtools_builder;
  devtools_builder.SetPath(normal_profile->GetPath());
  TestingProfile* devtools_profile = devtools_builder.BuildOffTheRecord(
      normal_profile, Profile::OTRProfileID::CreateUniqueForDevTools());
  ASSERT_FALSE(
      AvatarToolbarButtonInterface::CanShowForProfile(devtools_profile));

  TestBrowserEnvironment devtools_env(devtools_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton devtools_button(&devtools_env.delegate);
  NiceMock<MockAvatarObserver> observer;
  devtools_button.AddObserver(&observer);

  EXPECT_CALL(
      devtools_env.delegate,
      OnAvatarControlStateChanged(Pointee(
          Field(&toolbar_ui_api::mojom::AvatarControlState::enabled, false))));
  devtools_button.Initialize();
  EXPECT_FALSE(devtools_button.IsButtonEnabled());

  EXPECT_CALL(observer, OnButtonPressed()).Times(0);
  devtools_button.ButtonPressed(/*is_source_accelerator=*/false);
  devtools_button.ButtonPressed(/*is_source_accelerator=*/true);

  devtools_button.RemoveObserver(&observer);
}

TEST_F(WebUIAvatarToolbarButtonTest, GetWidthAndPreferredSizeChanged) {
  TestingProfile* normal_profile = CreateTestingProfile("normal");
  TestingProfile::Builder devtools_builder;
  devtools_builder.SetPath(normal_profile->GetPath());
  TestingProfile* devtools_profile = devtools_builder.BuildOffTheRecord(
      normal_profile, Profile::OTRProfileID::CreateUniqueForDevTools());
  ASSERT_FALSE(
      AvatarToolbarButtonInterface::CanShowForProfile(devtools_profile));

  TestBrowserEnvironment devtools_env(devtools_profile, widget_.get(), view_);
  ON_CALL(devtools_env.delegate, OnAvatarControlStateChanged)
      .WillByDefault(
          [&](toolbar_ui_api::mojom::AvatarControlStatePtr new_state) {
            devtools_env.toolbar_state.avatar_control_state =
                std::move(new_state);
          });
  WebUIAvatarToolbarButton devtools_button(&devtools_env.delegate);
  EXPECT_EQ(devtools_button.GetWidth(), 0);
  EXPECT_CALL(devtools_env.delegate, OnPreferredSizeChanged()).Times(0);
  devtools_button.Initialize();
  EXPECT_EQ(devtools_button.GetWidth(), 0);

  const int button_height =
      GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);

#if !BUILDFLAG(IS_CHROMEOS)
  TestBrowserEnvironment regular_env(normal_profile, widget_.get(), view_);
  ON_CALL(regular_env.delegate, OnAvatarControlStateChanged)
      .WillByDefault(
          [&](toolbar_ui_api::mojom::AvatarControlStatePtr new_state) {
            regular_env.toolbar_state.avatar_control_state =
                std::move(new_state);
          });
  WebUIAvatarToolbarButton regular_button(&regular_env.delegate);
  EXPECT_EQ(regular_button.GetWidth(), button_height);

  // Initial icon-only state matches the default `button_height` width.
  EXPECT_CALL(regular_env.delegate, OnPreferredSizeChanged()).Times(0);
  regular_button.Initialize();
  EXPECT_EQ(regular_button.GetWidth(), button_height);

  // Setting an explicit label widens the button and notifies the delegate.
  EXPECT_CALL(regular_env.delegate, OnPreferredSizeChanged()).Times(1);
  base::ScopedClosureRunner reset_explicit =
      regular_button.SetExplicitButtonState(u"Work", std::nullopt, std::nullopt,
                                            /*should_announce=*/false);
  const int labeled_width = regular_button.GetWidth();
  EXPECT_GT(labeled_width, button_height);
  testing::Mock::VerifyAndClearExpectations(&regular_env.delegate);

  // Updating another state field without changing the width pushes a new
  // `AvatarControlState` without firing `OnPreferredSizeChanged()`.
  regular_env.toolbar_state.controls_appear_inactive = true;
  EXPECT_CALL(regular_env.delegate, OnAvatarControlStateChanged).Times(1);
  EXPECT_CALL(regular_env.delegate, OnPreferredSizeChanged()).Times(0);
  regular_button.OnControlsAppearInactiveChanged();
  EXPECT_EQ(regular_button.GetWidth(), labeled_width);
  testing::Mock::VerifyAndClearExpectations(&regular_env.delegate);

  // Clearing the explicit label shrinks back and notifies the delegate again.
  EXPECT_CALL(regular_env.delegate, OnPreferredSizeChanged()).Times(1);
  reset_explicit.RunAndReset();
  EXPECT_EQ(regular_button.GetWidth(), button_height);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Across all platforms (including ChromeOS), initializing an Incognito avatar
  // button transitions from the default unlabeled width to the labeled chip
  // width and notifies `OnPreferredSizeChanged()`.
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(normal_profile);
  TestBrowserEnvironment incognito_env(incognito_profile, widget_.get(), view_);
  ON_CALL(incognito_env.delegate, OnAvatarControlStateChanged)
      .WillByDefault(
          [&](toolbar_ui_api::mojom::AvatarControlStatePtr new_state) {
            incognito_env.toolbar_state.avatar_control_state =
                std::move(new_state);
          });
  WebUIAvatarToolbarButton incognito_button(&incognito_env.delegate);
  EXPECT_EQ(incognito_button.GetWidth(), button_height);
  EXPECT_CALL(incognito_env.delegate, OnPreferredSizeChanged()).Times(1);
  incognito_button.Initialize();
  const int incognito_width = incognito_button.GetWidth();
  EXPECT_GT(incognito_width, button_height);
  testing::Mock::VerifyAndClearExpectations(&incognito_env.delegate);

  incognito_env.toolbar_state.controls_appear_inactive = true;
  EXPECT_CALL(incognito_env.delegate, OnAvatarControlStateChanged).Times(1);
  EXPECT_CALL(incognito_env.delegate, OnPreferredSizeChanged()).Times(0);
  incognito_button.OnControlsAppearInactiveChanged();
  EXPECT_EQ(incognito_button.GetWidth(), incognito_width);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(WebUIAvatarToolbarButtonTest, IsButtonEnabled_ChromeOSProfiles) {
  // Regular profile is disabled on ChromeOS.
  TestingProfile* normal_profile = CreateTestingProfile("normal");
  TestBrowserEnvironment normal_env(normal_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton normal_button(&normal_env.delegate);
  EXPECT_CALL(
      normal_env.delegate,
      OnAvatarControlStateChanged(Pointee(
          Field(&toolbar_ui_api::mojom::AvatarControlState::enabled, false))));
  normal_button.Initialize();
  EXPECT_FALSE(normal_button.IsButtonEnabled());

  // Guest OTR profile is disabled on ChromeOS.
  Profile* guest_otr_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/false);
  ASSERT_TRUE(guest_otr_profile);
  ASSERT_TRUE(guest_otr_profile->IsOffTheRecord());
  ASSERT_TRUE(guest_otr_profile->IsGuestSession());
  TestBrowserEnvironment guest_env(guest_otr_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton guest_button(&guest_env.delegate);
  EXPECT_CALL(
      guest_env.delegate,
      OnAvatarControlStateChanged(Pointee(
          Field(&toolbar_ui_api::mojom::AvatarControlState::enabled, false))));
  guest_button.Initialize();
  EXPECT_FALSE(guest_button.IsButtonEnabled());

  // Incognito OTR profile is enabled on ChromeOS.
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(normal_profile);
  TestBrowserEnvironment incognito_env(incognito_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton incognito_button(&incognito_env.delegate);
  EXPECT_CALL(incognito_env.delegate,
              OnAvatarControlStateChanged(Pointee(Field(
                  &toolbar_ui_api::mojom::AvatarControlState::enabled, true))));
  incognito_button.Initialize();
  EXPECT_TRUE(incognito_button.IsButtonEnabled());

  // Captive portal OTR profile (non-Guest) is disabled on ChromeOS.
  TestingProfile::Builder captive_builder;
  captive_builder.SetPath(normal_profile->GetPath());
  TestingProfile* captive_profile = captive_builder.BuildOffTheRecord(
      normal_profile, Profile::OTRProfileID::CreateUniqueForCaptivePortal());
  ASSERT_TRUE(captive_profile->IsOffTheRecord());
  ASSERT_FALSE(captive_profile->IsGuestSession());
  ASSERT_TRUE(captive_profile->GetOTRProfileID().IsCaptivePortal());
  TestBrowserEnvironment captive_env(captive_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton captive_button(&captive_env.delegate);
  EXPECT_CALL(
      captive_env.delegate,
      OnAvatarControlStateChanged(Pointee(
          Field(&toolbar_ui_api::mojom::AvatarControlState::enabled, false))));
  captive_button.Initialize();
  EXPECT_FALSE(captive_button.IsButtonEnabled());
}

TEST_F(WebUIAvatarToolbarButtonTest, ButtonPressedNoOpWhenDisabledOnChromeOS) {
  Profile* guest_otr_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/false);
  ASSERT_TRUE(guest_otr_profile);
  TestBrowserEnvironment guest_env(guest_otr_profile, widget_.get(), view_);
  WebUIAvatarToolbarButton guest_button(&guest_env.delegate);
  NiceMock<MockAvatarObserver> observer;
  guest_button.AddObserver(&observer);
  guest_button.Initialize();

  EXPECT_FALSE(guest_button.IsButtonEnabled());
  EXPECT_CALL(observer, OnButtonPressed()).Times(0);
  guest_button.ButtonPressed(/*is_source_accelerator=*/false);
  guest_button.ButtonPressed(/*is_source_accelerator=*/true);

  guest_button.RemoveObserver(&observer);
}

#endif
