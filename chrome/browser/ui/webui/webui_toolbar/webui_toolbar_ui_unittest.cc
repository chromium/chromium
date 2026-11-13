// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kApplyBoundProfileFn[] =
    "(window.__webuiToolbarApplyBoundProfile ?? "
    "(state => window.__webuiToolbarBoundProfile = state))";

// Stub config used while `PrewarmUnboundWebUIContents()` navigates in unit
// tests so `LoadURL()` does not create a second `WebUIToolbarUI` alongside the
// controller attached to `content::TestWebUI`.
class StubWebUIToolbarConfig : public WebUIToolbarConfig {
 public:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<ui::MojoWebUIController>(
        web_ui, /*enable_chrome_send=*/true);
  }
};

// Test fixture for WebUIToolbarUIConfig.
class WebUIToolbarUIConfigTest : public testing::Test {
 public:
  WebUIToolbarUIConfigTest() = default;
  ~WebUIToolbarUIConfigTest() override = default;

  // Not movable or copyable.
  WebUIToolbarUIConfigTest(const WebUIToolbarUIConfigTest&) = delete;
  WebUIToolbarUIConfigTest& operator=(const WebUIToolbarUIConfigTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the IsWebUIEnabled method returns the correct value based on the
// feature flag.
TEST_F(WebUIToolbarUIConfigTest, IsWebUIEnabled) {
  TestingProfile profile;
  WebUIToolbarConfig config;

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton},
        {features::kWebUISplitTabsButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUISplitTabsButton},
        {features::kWebUIReloadButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI},
        {features::kWebUIReloadButton, features::kWebUISplitTabsButton});
    EXPECT_FALSE(config.IsWebUIEnabled(&profile));
  }
}

TEST(WebUIToolbarUITest, KnownElementIdentifiersIncludeGlicSplitElements) {
  const auto known_ids = WebUIToolbarUI::GetKnownElementIdentifiers();
  EXPECT_TRUE(std::ranges::contains(known_ids, kGlicButtonElementId));
  EXPECT_TRUE(std::ranges::contains(known_ids, kGlicActorTaskIconElementId));
}

class WebUIToolbarUIUnboundTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIToolbarUIUnboundTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kTopChromeWebUISharedRenderer, features::kGlic,
         features::kGlicHorizontalTabToolbarButton},
        {features::kGlicCountryFiltering, features::kGlicLocaleFiltering});
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);
    glic::GlicEnabling::SetSystemRequirementMetForTesting(true);
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_top_chrome_profile_ =
        std::make_unique<webui::ScopedTopChromeWebUIProfileForTesting>();
  }

  void TearDown() override {
    scoped_top_chrome_profile_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
    glic::GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
  }

 protected:
  struct UnboundToolbarHarness {
    std::unique_ptr<content::WebContents> contents;
    content::TestWebUI test_web_ui;
    raw_ptr<WebUIToolbarUI> toolbar_ui = nullptr;
    raw_ptr<content::RenderFrameHost> rfh = nullptr;
  };

  std::unique_ptr<UnboundToolbarHarness> CreateUnboundToolbar(
      Profile& template_profile,
      bool create_frame = true,
      bool commit = true) {
    auto harness = std::make_unique<UnboundToolbarHarness>();
    {
      content::ScopedWebUIConfigRegistration stub_config(
          std::make_unique<StubWebUIToolbarConfig>());
      harness->contents = waap::PrewarmHelper::PrewarmUnboundWebUIContents(
          template_profile, [&](content::WebContents& contents) {
            WebUIToolbarUnboundSeedUserData::CreateForWebContents(
                &contents, template_profile);
          });
    }
    CHECK(harness->contents);
    harness->rfh = harness->contents->GetPrimaryMainFrame();
    harness->test_web_ui.set_web_contents(harness->contents.get());
    harness->test_web_ui.set_render_frame_host(harness->rfh);
    harness->test_web_ui.SetController(
        std::make_unique<WebUIToolbarUI>(&harness->test_web_ui));
    harness->toolbar_ui =
        harness->test_web_ui.GetController()->GetAs<WebUIToolbarUI>();
    CHECK(harness->toolbar_ui);
    if (create_frame) {
      harness->toolbar_ui->WebUIRenderFrameCreated(harness->rfh);
    }
    if (commit) {
      Commit(*harness);
    }
    return harness;
  }

  void Commit(UnboundToolbarHarness& harness) {
    content::MockNavigationHandle nav_handle(
        GURL(chrome::kChromeUIWebUIToolbarURL), harness.rfh);
    nav_handle.set_has_committed(true);
    nav_handle.set_is_same_document(false);
    harness.toolbar_ui->DidFinishNavigation(&nav_handle);
  }

  bool BindAndApply(UnboundToolbarHarness& harness, Profile& target_profile) {
    const bool colors_changed =
        waap::PrewarmHelper::BindUserProfile(*harness.contents, target_profile);
    harness.toolbar_ui->OnUserProfileBoundForTesting(target_profile,
                                                     colors_changed);
    return colors_changed;
  }

 private:
  views::test::TestLayoutProvider layout_provider_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<webui::ScopedTopChromeWebUIProfileForTesting>
      scoped_top_chrome_profile_;
};

// Verifies that an unbound WebUIToolbarUI seeded from a template profile
// populates `BuildInitialState()` and its color provider from that seed, clears
// generic font family maps while unbound, skips in-process colors.css, removes
// the seed user data on bind, and sends no JS message when bound to a profile
// whose state matches the seed.
TEST_F(WebUIToolbarUIUnboundTest,
       SeededInitialStateAndNoMessageWhenNothingDiffers) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  const ui::ColorProviderKey baseline_key =
      ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(nullptr);
  ThemeServiceFactory::GetForProfile(profile())->SetBrowserColorScheme(
      baseline_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
          ? ThemeService::BrowserColorScheme::kLight
          : ThemeService::BrowserColorScheme::kDark);
  const ui::ColorProviderKey seeded_key =
      waap::PrewarmHelper::GetColorProviderKey(profile());
  const auto expected_eligibility =
      WebUIToolbarUI::GetProfileButtonEligibility(profile());

  std::unique_ptr<UnboundToolbarHarness> harness =
      CreateUnboundToolbar(*profile());
  ASSERT_FALSE(webui::GetUserProfile(harness->contents.get()));
  EXPECT_TRUE(harness->contents->GetOrCreateWebPreferences()
                  .standard_font_family_map.empty());
  EXPECT_EQ(seeded_key.color_mode, harness->contents->GetColorMode());
  EXPECT_TRUE(WebUIToolbarUnboundSeedUserData::FromWebContents(
      harness->contents.get()));

  base::DictValue initial_state =
      harness->toolbar_ui->BuildInitialStateForTesting();
  EXPECT_EQ(true, initial_state.FindBool("isUnbound"));
  EXPECT_EQ(expected_eligibility.is_profile_eligible_for_glic,
            initial_state.FindBool("isProfileEligibleForGlic"));
  EXPECT_EQ(expected_eligibility.can_show_avatar_for_profile,
            initial_state.FindBool("canShowAvatarForProfile"));
  EXPECT_EQ(true, initial_state.FindBool("homeButtonShouldBeShown"));

  blink::mojom::LocalResourceLoaderConfig config;
  const url::Origin toolbar_origin =
      url::Origin::Create(GURL(chrome::kChromeUIWebUIToolbarURL));
  harness->toolbar_ui->PopulateLocalResourceLoaderConfig(&config,
                                                         toolbar_origin);
  EXPECT_FALSE(
      config.sources.contains(url::Origin::Create(GURL("chrome://theme/"))));
  EXPECT_TRUE(config.sources.contains(toolbar_origin));

  // Binding to the matching profile removes the seed user data and sends no
  // message.
  EXPECT_FALSE(BindAndApply(*harness, *profile()));
  EXPECT_FALSE(WebUIToolbarUnboundSeedUserData::FromWebContents(
      harness->contents.get()));
  EXPECT_TRUE(harness->test_web_ui.call_data().empty());
}

// Verifies that binding after commit sends only the fields that
// differ from the seed, and includes `colorsCss` if and only if
// `colors_changed` is true.
TEST_F(WebUIToolbarUIUnboundTest,
       SendsOnlyDiffsAndColorsCssOnlyOnColorKeyChange) {
  // 1. Button pref differs, color provider key matches -> no `colorsCss`.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, false);
    std::unique_ptr<UnboundToolbarHarness> harness =
        CreateUnboundToolbar(*profile());

    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
    EXPECT_FALSE(BindAndApply(*harness, *profile()));
    ASSERT_EQ(1u, harness->test_web_ui.call_data().size());
    EXPECT_EQ(kApplyBoundProfileFn,
              harness->test_web_ui.call_data()[0]->function_name());
    const base::Value* arg = harness->test_web_ui.call_data()[0]->arg1();
    ASSERT_TRUE(arg && arg->is_dict());
    const base::DictValue& bound_dict = arg->GetDict();
    EXPECT_EQ(1u, bound_dict.size());
    EXPECT_EQ(true, bound_dict.FindBool("homeButtonShouldBeShown"));
    EXPECT_FALSE(bound_dict.contains("colorsCss"));
  }

  // 2. Color provider key differs, button fields match -> `colorsCss` only.
  {
    std::unique_ptr<UnboundToolbarHarness> harness =
        CreateUnboundToolbar(*profile());

    const ui::ColorProviderKey seeded_key =
        waap::PrewarmHelper::GetColorProviderKey(profile());
    ThemeServiceFactory::GetForProfile(profile())->SetBrowserColorScheme(
        seeded_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
            ? ThemeService::BrowserColorScheme::kLight
            : ThemeService::BrowserColorScheme::kDark);

    EXPECT_TRUE(BindAndApply(*harness, *profile()));
    ASSERT_EQ(1u, harness->test_web_ui.call_data().size());
    const base::Value* arg = harness->test_web_ui.call_data()[0]->arg1();
    ASSERT_TRUE(arg && arg->is_dict());
    const base::DictValue& bound_dict = arg->GetDict();
    EXPECT_EQ(1u, bound_dict.size());
    const std::string* colors_css = bound_dict.FindString("colorsCss");
    ASSERT_TRUE(colors_css);
    EXPECT_FALSE(colors_css->empty());
  }
}

// Verifies the pre-commit bind cases:
// - Bind after `WebUIRenderFrameCreated()` and before commit overwrites
//   `initialState` (keeping `isUnbound: true` and adding `colorsCss`) and
//   sends the diff at commit.
// - Bind before `WebUIRenderFrameCreated()` sends nothing and lets frame
//   creation read the bound profile directly.
TEST_F(WebUIToolbarUIUnboundTest,
       PreCommitBindOverwritesInitialStateAndSendsAtCommit) {
  // 1. Bind after frame creation and before commit.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, false);
    std::unique_ptr<UnboundToolbarHarness> harness =
        CreateUnboundToolbar(*profile(), /*create_frame=*/true,
                             /*commit=*/false);

    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
    const ui::ColorProviderKey seeded_key =
        waap::PrewarmHelper::GetColorProviderKey(profile());
    ThemeServiceFactory::GetForProfile(profile())->SetBrowserColorScheme(
        seeded_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
            ? ThemeService::BrowserColorScheme::kLight
            : ThemeService::BrowserColorScheme::kDark);

    EXPECT_TRUE(BindAndApply(*harness, *profile()));
    EXPECT_TRUE(harness->test_web_ui.call_data().empty());

    // Overwritten `initialState` carries bound values, `isUnbound: true`, and
    // `colorsCss` for a page that has not read `initialState` yet.
    base::DictValue overwritten =
        harness->toolbar_ui->BuildInitialStateForTesting();
    EXPECT_EQ(true, overwritten.FindBool("isUnbound"));
    EXPECT_EQ(true, overwritten.FindBool("homeButtonShouldBeShown"));
    const std::string* overwritten_css = overwritten.FindString("colorsCss");
    ASSERT_TRUE(overwritten_css);
    EXPECT_FALSE(overwritten_css->empty());

    // Commit also sends the diff for a page that reads `initialState` before
    // the overwrite lands.
    Commit(*harness);
    ASSERT_EQ(1u, harness->test_web_ui.call_data().size());
    EXPECT_EQ(kApplyBoundProfileFn,
              harness->test_web_ui.call_data()[0]->function_name());
    const base::DictValue& flushed =
        harness->test_web_ui.call_data()[0]->arg1()->GetDict();
    EXPECT_EQ(true, flushed.FindBool("homeButtonShouldBeShown"));
    EXPECT_TRUE(flushed.FindString("colorsCss"));
  }

  // 2. Bind before `WebUIRenderFrameCreated()` sends nothing.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, false);
    std::unique_ptr<UnboundToolbarHarness> harness =
        CreateUnboundToolbar(*profile(), /*create_frame=*/false,
                             /*commit=*/false);

    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
    BindAndApply(*harness, *profile());
    EXPECT_FALSE(WebUIToolbarUnboundSeedUserData::FromWebContents(
        harness->contents.get()));

    harness->toolbar_ui->WebUIRenderFrameCreated(harness->rfh);
    base::DictValue state_at_frame_creation =
        harness->toolbar_ui->BuildInitialStateForTesting();
    EXPECT_FALSE(state_at_frame_creation.contains("isUnbound"));
    EXPECT_FALSE(state_at_frame_creation.contains("colorsCss"));
    EXPECT_EQ(true,
              state_at_frame_creation.FindBool("homeButtonShouldBeShown"));

    Commit(*harness);
    EXPECT_TRUE(harness->test_web_ui.call_data().empty());
  }
}

// Verifies button-eligibility and home-button diffs when binding a Guest
// profile to a regular-seeded spare and a secondary regular profile to a
// Guest-seeded spare.
TEST_F(WebUIToolbarUIUnboundTest,
       GuestAndSecondaryProfileButtonEligibilityPermutations) {
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_parent = guest_builder.Build();
  Profile* guest_otr =
      guest_parent->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(guest_otr);

  const auto guest_eligibility =
      WebUIToolbarUI::GetProfileButtonEligibility(guest_otr);
  EXPECT_FALSE(guest_eligibility.is_profile_eligible_for_glic);

  // 1. Spare seeded from regular `profile()` (with Glic eligible), bound to
  // Guest OTR profile (where Glic is ineligible).
  {
    profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
    std::unique_ptr<UnboundToolbarHarness> harness;
    {
      glic::GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_glic;
      harness = CreateUnboundToolbar(*profile());
    }

    BindAndApply(*harness, *guest_otr);
    ASSERT_EQ(1u, harness->test_web_ui.call_data().size());
    const base::DictValue& diff =
        harness->test_web_ui.call_data()[0]->arg1()->GetDict();
    EXPECT_EQ(false, diff.FindBool("isProfileEligibleForGlic"));
    EXPECT_EQ(false, diff.FindBool("homeButtonShouldBeShown"));
  }

  // 2. Spare seeded from Guest's original profile, bound to a secondary regular
  // profile (with Glic eligible and `kShowHomeButton` enabled).
  {
    TestingProfile secondary_profile;
    secondary_profile.GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

    std::unique_ptr<UnboundToolbarHarness> harness =
        CreateUnboundToolbar(*guest_parent);

    {
      glic::GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_glic;
      BindAndApply(*harness, secondary_profile);
    }
    ASSERT_EQ(1u, harness->test_web_ui.call_data().size());
    const base::DictValue& diff =
        harness->test_web_ui.call_data()[0]->arg1()->GetDict();
    EXPECT_EQ(true, diff.FindBool("isProfileEligibleForGlic"));
    EXPECT_EQ(true, diff.FindBool("homeButtonShouldBeShown"));
  }
}

}  // namespace
