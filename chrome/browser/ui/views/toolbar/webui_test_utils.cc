// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser) {
  if (!browser || (!features::IsWebUIToolbarEnabled() &&
                   !base::FeatureList::IsEnabled(
                       features::kWebUIToolbarProcessOverheadExperiment))) {
    return;
  }

  base::RunLoop run_loop;
  BrowserElements* browser_elements = BrowserElements::From(browser);
  if (!browser_elements) {
    return;
  }
  ui::TrackedElement* element =
      browser_elements->GetElement(kWebUIToolbarElementIdentifier);
  if (!element) {
    return;
  }
  WebUIToolbarWebView* webui_toolbar = views::AsViewClass<WebUIToolbarWebView>(
      element->AsA<views::TrackedElementViews>()->view());
  if (!webui_toolbar) {
    return;
  }

  webui_toolbar->SetDidFirstNonEmptyPaintCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

void WaitForInitialWebUIToolbar(BrowserWindowInterface* browser) {
  base::RunLoop run_loop;
  InitialWebUIManager* manager = InitialWebUIManager::From(browser);
  if (!manager || !manager->RequestDeferShow(run_loop.QuitClosure())) {
    return;
  }
  run_loop.Run();
}

AvatarToolbarButtonTestAccessor::AvatarToolbarButtonTestAccessor(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  WaitForAvatarButton();
}

void AvatarToolbarButtonTestAccessor::WaitForAvatarButton() {
  ui::ElementContext context;
  if (BrowserElements* browser_elements = BrowserElements::From(browser_)) {
    context = browser_elements->GetContext();
  } else {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (!browser_view) {
      return;
    }
    context = views::ElementTrackerViews::GetContextForView(browser_view);
  }

  if (ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kToolbarAvatarButtonElementId, context)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // The avatar button is only added to normal browsers (those with a tab
  // strip).
  if (Browser* const browser_ptr = browser_->GetBrowserForMigrationOnly();
      !browser_ptr || !browser_ptr->SupportsWindowFeature(
                          Browser::WindowFeature::kFeatureTabStrip)) {
    return;
  }
#endif

  Profile* const profile = browser_->GetProfile();
  bool show_avatar_toolbar_button = true;
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS only badges Incognito, Guest, and captive portal signin icons in
  // the browser window.
  show_avatar_toolbar_button = profile->IsIncognitoProfile() ||
                               profile->IsGuestSession() ||
                               (profile->IsOffTheRecord() &&
                                profile->GetOTRProfileID().IsCaptivePortal());
#else
  // DevTools profiles are OffTheRecord, so hide it there.
  show_avatar_toolbar_button = profile->IsIncognitoProfile() ||
                               profile->IsGuestSession() ||
                               profile->IsRegularProfile();
#endif

  if (!show_avatar_toolbar_button) {
    return;
  }

  base::RunLoop run_loop;
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kToolbarAvatarButtonElementId, context,
          base::BindRepeating(
              [](base::RunLoop* run_loop, ui::TrackedElement* element) {
                run_loop->Quit();
              },
              &run_loop));
  run_loop.Run();
}

AvatarToolbarButtonInterface* AvatarToolbarButtonTestAccessor::GetInterface() {
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return nullptr;
  }
  return browser_view->toolbar_button_provider()
      ->GetAvatarToolbarButtonInterface();
}

AvatarToolbarButton* AvatarToolbarButtonTestAccessor::GetButton() {
  DCHECK(!features::IsWebUIAvatarButtonEnabled());
  return static_cast<AvatarToolbarButton*>(GetInterface());
}

bool AvatarToolbarButtonTestAccessor::GetEnabled() {
  if (AvatarToolbarButton* button = GetButton()) {
    return button->GetEnabled();
  }
  return false;
}

bool AvatarToolbarButtonTestAccessor::GetVisible() {
  if (AvatarToolbarButton* button = GetButton()) {
    return button->GetVisible();
  }
  return false;
}

std::u16string AvatarToolbarButtonTestAccessor::GetText() {
  if (AvatarToolbarButton* button = GetButton()) {
    return std::u16string(button->GetText());
  }
  return std::u16string();
}

void AvatarToolbarButtonTestAccessor::Click() {
  if (AvatarToolbarButton* button = GetButton()) {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        button, ui::test::InteractionTestUtil::InputType::kMouse);
  }
}

views::Widget* AvatarToolbarButtonTestAccessor::GetWidget() {
  if (AvatarToolbarButton* button = GetButton()) {
    return button->GetWidget();
  }
  return nullptr;
}

gfx::ImageSkia AvatarToolbarButtonTestAccessor::GetImage(
    views::Button::ButtonState state) {
  if (AvatarToolbarButton* button = GetButton()) {
    return button->GetImage(state);
  }
  return gfx::ImageSkia();
}

std::u16string AvatarToolbarButtonTestAccessor::GetRenderedTooltipText(
    const gfx::Point& p) {
  if (AvatarToolbarButton* button = GetButton()) {
    return button->GetRenderedTooltipText(p);
  }
  return std::u16string();
}
