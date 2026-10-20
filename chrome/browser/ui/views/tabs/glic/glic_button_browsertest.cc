// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_utils.h"

namespace glic {
namespace {

class GlicButtonTest : public InProcessBrowserTest {
 public:
  GlicButtonTest()
      : glic_test_env_({},
                       GetDefaultEnabledGlicTestFeatures(),
                       GetDisabledFeatures()) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  glic::TabStripGlicButton* glic_button() {
    return views::AsViewClass<glic::TabStripGlicButton>(
        glic::GlicButtonInterface::FromBrowser(browser()));
  }

  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
  }

  void WaitForGlicPanelShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_service()->instance_coordinator().IsAnyPanelShowing();
    })) << "Glic panel should have been shown";
  }

 private:
  static std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    std::vector<base::test::FeatureRef> disabled =
        GetDefaultDisabledGlicTestFeatures();
    disabled.push_back(features::kGlicHorizontalTabToolbarButton);
    return disabled;
  }

  GlicTestEnvironment glic_test_env_;
};

// Returns true if `button`'s normal-state icon is the image resource
// `resource_id`. ResourceBundle caches one ImageSkia per resource id, so
// comparing backing objects identifies which resource was selected. This
// matters because in unbranded builds the Glic and GEiC resources
// intentionally point at the same placeholder art, so a pixel comparison would
// pass even if the wrong resource were chosen.
bool NormalIconIsResource(views::LabelButton* button, int resource_id) {
  const std::optional<ui::ImageModel>& model =
      button->GetImageModel(views::Button::STATE_NORMAL);
  if (!model.has_value() || !model->IsImage()) {
    return false;
  }
  const gfx::ImageSkia* expected =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  return expected &&
         model->GetImage().AsImageSkia().BackedBySameObjectAs(*expected);
}

// By default the button shows the Glic spark.
IN_PROC_BROWSER_TEST_F(GlicButtonTest, NormalIconIsGlicIcon) {
  ASSERT_TRUE(glic_button());
  EXPECT_TRUE(NormalIconIsResource(glic_button(), IDR_GLIC_BUTTON_ALT_ICON));
  auto* service = glic::GlicKeyedService::Get(browser()->GetProfile());
  ASSERT_TRUE(service);
  EXPECT_EQ(service->provider_spec().GetSettingsSection(),
            glic::GlicSettingsSection::kGemini);
  EXPECT_EQ(service->provider_spec().GetSettingsSubpage(),
            chrome::kGlicSettingsSubpage);
  EXPECT_EQ(service->provider_spec().GetEntryPointIconId(),
            IDR_GLIC_BUTTON_ALT_ICON);
}

// With Gemini Enterprise enabled for the profile, the button shows the GEiC
// icon instead.
class GlicButtonGeicIconTest : public GlicButtonTest {
 public:
  GlicButtonGeicIconTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGeic,
        {{"geic-guest-url", "https://business.gemini.google/panel"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicButtonGeicIconTest, NormalIconIsGeicIcon) {
  ASSERT_TRUE(glic_button());
  EXPECT_TRUE(NormalIconIsResource(glic_button(), IDR_GEIC_BUTTON_ICON));
  auto* service = glic::GlicKeyedService::Get(browser()->GetProfile());
  ASSERT_TRUE(service);
  EXPECT_EQ(service->provider_spec().GetSettingsSection(),
            glic::GlicSettingsSection::kGeminiEnterprise);
  EXPECT_EQ(service->provider_spec().GetSettingsSubpage(),
            chrome::kGlicEnterpriseSettingsSubpage);
  EXPECT_EQ(service->provider_spec().GetEntryPointIconId(),
            IDR_GEIC_BUTTON_ICON);
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuPinned) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_TRUE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuUnpinned) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, false);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_FALSE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, UnpinCommand) {
  PrefService* profile_prefs = browser()->GetProfile()->GetPrefs();
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ExecuteCommand(IDC_GLIC_TOGGLE_PIN, ui::EF_NONE);
  EXPECT_FALSE(profile_prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, TooltipAndA11yTextForOpening) {
  EXPECT_FALSE(glic_service()->instance_coordinator().IsAnyPanelShowing());
  EXPECT_EQ(glic_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP));
}

// Tests using programmatic window activation are flaky on Linux.
// TODO(crbug.com/428742560): De-flake for Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_TooltipAndA11yTextWhileGlicWindowOpen \
  DISABLED_TooltipAndA11yTextWhileGlicWindowOpen
#else
#define MAYBE_TooltipAndA11yTextWhileGlicWindowOpen \
  TooltipAndA11yTextWhileGlicWindowOpen
#endif  // BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(GlicButtonTest,
                       MAYBE_TooltipAndA11yTextWhileGlicWindowOpen) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  // Toggle to open the glic window.
  glic_service()->ToggleUI(browser(), false,
                           mojom::InvocationSource::kTopChromeButton);
  WaitForGlicPanelShow();

  EXPECT_EQ(glic_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE));
  EXPECT_EQ(glic_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE));
}

class TestCustomThemeSupplier : public CustomThemeSupplier {
 public:
  TestCustomThemeSupplier() : CustomThemeSupplier(ThemeType::kExtension) {}

  void SetColor(int id, SkColor color) override { colors_[id] = color; }

  bool GetColor(int id, SkColor* color) const override {
    auto it = colors_.find(id);
    if (it != colors_.end()) {
      *color = it->second;
      return true;
    }
    return false;
  }

 protected:
  ~TestCustomThemeSupplier() override = default;

 private:
  std::map<int, SkColor> colors_;
};
class FakeThemeService : public ThemeService {
 public:
  explicit FakeThemeService(Profile* profile)
      : ThemeService(profile, GetFakeThemeHelper()) {}

  CustomThemeSupplier* GetThemeSupplier() const override {
    return theme_supplier_ ? theme_supplier_.get()
                           : ThemeService::GetThemeSupplier();
  }

  bool UsingExtensionTheme() const override {
    return is_using_extension_theme_ || ThemeService::UsingExtensionTheme();
  }
  void set_using_extension_theme(bool value) {
    is_using_extension_theme_ = value;
  }

  void SetCustomThemeSupplier(
      scoped_refptr<CustomThemeSupplier> theme_supplier) {
    theme_supplier_ = std::move(theme_supplier);
  }

 private:
  static const ThemeHelper& GetFakeThemeHelper() {
    static base::NoDestructor<ThemeHelper> helper;
    return *helper;
  }

  scoped_refptr<CustomThemeSupplier> theme_supplier_;
  bool is_using_extension_theme_ = false;
};

class GlicButtonThemeTest : public GlicButtonTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    GlicButtonTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  ThemeServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
                        auto service = std::make_unique<FakeThemeService>(
                            static_cast<Profile*>(context));
                        service->Init();
                        return service;
                      }));
                }));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicButtonThemeTest, CustomThemeFallback) {
  FakeThemeService* fake_theme_service = static_cast<FakeThemeService*>(
      ThemeServiceFactory::GetForProfile(browser()->GetProfile()));
  ASSERT_TRUE(fake_theme_service);

  // 1. By default, the button should not use the fallback text color.
  EXPECT_NE(glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL),
            glic_button()->GetColorProvider()->GetColor(
                kColorTabForegroundInactiveFrameActive));

  // 2. Enable the custom theme fallback in the fake theme service.
  fake_theme_service->set_using_extension_theme(true);

  // Trigger OnThemeChanged to update the button colors.
  glic_button()->OnThemeChanged();

  // 3. Verify that the button now uses the custom theme fallback color.
  EXPECT_EQ(glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL),
            glic_button()->GetColorProvider()->GetColor(
                kColorTabForegroundInactiveFrameActive));
}

IN_PROC_BROWSER_TEST_F(GlicButtonThemeTest,
                       CustomThemeDarkBackgroundHasLightTextColor) {
  FakeThemeService* fake_theme_service = static_cast<FakeThemeService*>(
      ThemeServiceFactory::GetForProfile(browser()->GetProfile()));
  ASSERT_TRUE(fake_theme_service);

  auto theme_supplier = base::MakeRefCounted<TestCustomThemeSupplier>();
  theme_supplier->SetColor(ThemeProperties::COLOR_FRAME_ACTIVE, SK_ColorBLACK);
  theme_supplier->SetColor(
      ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
      SkColorSetRGB(0x20, 0x20, 0x20));
  fake_theme_service->set_using_extension_theme(true);
  fake_theme_service->SetCustomThemeSupplier(theme_supplier);
  glic_button()->OnThemeChanged();

  const SkColor text_color =
      glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL);
  const SkColor expected_fg_color = glic_button()->GetColorProvider()->GetColor(
      kColorTabForegroundInactiveFrameActive);
  EXPECT_EQ(text_color, expected_fg_color);

  // The text color must be light and contrast adequately with the dark
  // background.
  const SkColor bg_color = glic_button()->GetColorProvider()->GetColor(
      kColorNewTabButtonCRBackgroundFrameActive);
  EXPECT_FALSE(color_utils::IsDark(text_color));
  EXPECT_GE(color_utils::GetContrastRatio(text_color, bg_color),
            color_utils::kMinimumReadableContrastRatio);
}

class ToolbarGlicButtonThemeTest : public InProcessBrowserTest {
 public:
  ToolbarGlicButtonThemeTest()
      : glic_test_env_({},
                       GetEnabledFeatures(),
                       GetDefaultDisabledGlicTestFeatures()) {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  ThemeServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
                        auto service = std::make_unique<FakeThemeService>(
                            static_cast<Profile*>(context));
                        service->Init();
                        return service;
                      }));
                }));
  }

 protected:
  glic::ToolbarGlicButton* glic_button() {
    return views::AsViewClass<glic::ToolbarGlicButton>(
        glic::GlicButtonInterface::FromBrowser(browser()));
  }

 private:
  static std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    std::vector<base::test::FeatureRef> enabled =
        GetDefaultEnabledGlicTestFeatures();
    enabled.push_back(features::kGlicHorizontalTabToolbarButton);
    return enabled;
  }

  GlicTestEnvironment glic_test_env_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ToolbarGlicButtonThemeTest, CustomThemeFallback) {
  FakeThemeService* fake_theme_service = static_cast<FakeThemeService*>(
      ThemeServiceFactory::GetForProfile(browser()->GetProfile()));
  ASSERT_TRUE(fake_theme_service);
  ASSERT_TRUE(glic_button());

  // 1. By default, the button should not use the fallback text color.
  EXPECT_NE(glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL),
            glic_button()->GetColorProvider()->GetColor(
                kColorAvatarButtonHighlightDefaultForeground));

  // 2. Enable the custom theme fallback in the fake theme service.
  fake_theme_service->set_using_extension_theme(true);

  // Trigger OnThemeChanged to update the button colors.
  glic_button()->OnThemeChanged();

  // 3. Verify that the button now uses the custom theme fallback color.
  EXPECT_EQ(glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL),
            glic_button()->GetColorProvider()->GetColor(
                kColorAvatarButtonHighlightDefaultForeground));
}

IN_PROC_BROWSER_TEST_F(ToolbarGlicButtonThemeTest,
                       CustomThemeDarkBackgroundHasLightTextColor) {
  FakeThemeService* fake_theme_service = static_cast<FakeThemeService*>(
      ThemeServiceFactory::GetForProfile(browser()->GetProfile()));
  ASSERT_TRUE(fake_theme_service);
  ASSERT_TRUE(glic_button());

  auto theme_supplier = base::MakeRefCounted<TestCustomThemeSupplier>();
  theme_supplier->SetColor(ThemeProperties::COLOR_FRAME_ACTIVE, SK_ColorBLACK);
  theme_supplier->SetColor(
      ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
      SkColorSetRGB(0x20, 0x20, 0x20));
  fake_theme_service->set_using_extension_theme(true);
  fake_theme_service->SetCustomThemeSupplier(theme_supplier);
  glic_button()->OnThemeChanged();

  const SkColor text_color =
      glic_button()->GetTextColorForTesting(views::Button::STATE_NORMAL);
  const SkColor expected_fg_color = glic_button()->GetColorProvider()->GetColor(
      kColorAvatarButtonHighlightDefaultForeground);
  EXPECT_EQ(text_color, expected_fg_color);

  // The text color must be light and contrast adequately with the dark
  // background.
  const SkColor bg_color = glic_button()->GetColorProvider()->GetColor(
      kColorAvatarButtonHighlightDefault);
  EXPECT_FALSE(color_utils::IsDark(text_color));
  EXPECT_GE(color_utils::GetContrastRatio(text_color, bg_color),
            color_utils::kMinimumReadableContrastRatio);
}

}  // namespace
}  // namespace glic
