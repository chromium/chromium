// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/events/event_constants.h"

namespace glic {
namespace {

class GlicButtonTest : public InProcessBrowserTest {
 public:
  GlicButtonTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  glic::GlicButton* glic_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<glic::GlicButton>(
        kGlicButtonElementId);
  }

  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  }

  void WaitForFreShownAndInitialized() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_service()
          ->fre_controller()
          .IsShowingDialogAndStateInitialized();
    })) << "FRE dialog should have been shown";
  }

  void WaitForGlicPanelShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_service()->IsWindowShowing();
    })) << "Glic panel should have been shown";
  }

 private:
  GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuPinned) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_TRUE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, ContextMenuUnpinned) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, false);

  glic_button()->ShowContextMenuForViewImpl(glic_button(), gfx::Point(),
                                            ui::mojom::MenuSourceType::kMouse);
  EXPECT_FALSE(glic_button()->IsContextMenuShowingForTest());
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, UnpinCommand) {
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);

  glic_button()->ExecuteCommand(IDC_GLIC_TOGGLE_PIN, ui::EF_NONE);
  EXPECT_FALSE(profile_prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, TooltipAndA11yTextForOpening) {
  EXPECT_FALSE(glic_service()->IsWindowOrFreShowing());
  EXPECT_EQ(glic_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP));
}

IN_PROC_BROWSER_TEST_F(GlicButtonTest, TooltipAndA11yTextWhileGlicFreOpen) {
  // Toggle to open the FRE dialog.
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
  glic_service()->ToggleUI(browser(), false,
                           mojom::InvocationSource::kTopChromeButton);
  WaitForFreShownAndInitialized();

  EXPECT_EQ(glic_button()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE));
  EXPECT_EQ(glic_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE));
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
}  // namespace
}  // namespace glic
