// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include <memory>

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/touch_selection_menu/touch_selection_menu_runner_chromeos.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget_delegate.h"
#endif

// Helpers --------------------------------------------------------------------

namespace {

Profile* GetProfileForWindow(const views::Widget* window) {
  if (!window)
    return nullptr;
  return reinterpret_cast<Profile*>(
      window->GetNativeWindowProperty(Profile::kProfileKey));
}

// If the given window has a profile associated with it, use that profile's
// preference service. Otherwise, store and retrieve the data from Local State.
// This function may return NULL if the necessary pref service has not yet
// been initialized.
// TODO(mirandac): This function will also separate windows by profile in a
// multi-profile environment.
PrefService* GetPrefsForWindow(const views::Widget* window) {
  Profile* profile = GetProfileForWindow(window);
  if (!profile) {
    // Use local state for windows that have no explicit profile.
    return g_browser_process->local_state();
  }
  return profile->GetPrefs();
}

}  // namespace

// ChromeViewsDelegate --------------------------------------------------------

ChromeViewsDelegate::ChromeViewsDelegate() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ViewsDelegate's constructor may have created a menu runner already, and
  // since TouchSelectionMenuRunner is a singleton with checks to not
  // initialize it if there is already an existing runner we need to first
  // destroy runner before we can create the ChromeOS specific instance.
  SetTouchSelectionMenuRunner(nullptr);
  SetTouchSelectionMenuRunner(
      std::make_unique<TouchSelectionMenuRunnerChromeOS>());
#endif
}

ChromeViewsDelegate::~ChromeViewsDelegate() {
  DCHECK_EQ(0u, ref_count_);
}

void ChromeViewsDelegate::SaveWindowPlacement(
    const views::Widget* window,
    const std::string& window_name,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state) {
  PrefService* prefs = GetPrefsForWindow(window);
  if (!prefs)
    return;

  std::unique_ptr<ScopedDictPrefUpdate> pref_update;
  base::Value::Dict& window_preferences =
      chrome::GetWindowPlacementDictionaryReadWrite(window_name, prefs,
                                                    pref_update);
  window_preferences.Set("left", bounds.x());
  window_preferences.Set("top", bounds.y());
  window_preferences.Set("right", bounds.right());
  window_preferences.Set("bottom", bounds.bottom());
  window_preferences.Set("maximized",
                         show_state == ui::mojom::WindowShowState::kMaximized);

  gfx::Rect work_area(display::Screen::GetScreen()
                          ->GetDisplayNearestView(window->GetNativeView())
                          .work_area());
  window_preferences.Set("work_area_left", work_area.x());
  window_preferences.Set("work_area_top", work_area.y());
  window_preferences.Set("work_area_right", work_area.right());
  window_preferences.Set("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowPlacement(
    const views::Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return false;

  DCHECK(prefs->FindPreference(window_name));
  const base::Value::Dict& dictionary = prefs->GetDict(window_name);
  std::optional<int> left = dictionary.FindInt("left");
  std::optional<int> top = dictionary.FindInt("top");
  std::optional<int> right = dictionary.FindInt("right");
  std::optional<int> bottom = dictionary.FindInt("bottom");
  if (!left || !top || !right || !bottom)
    return false;

  bounds->SetRect(*left, *top, *right - *left, *bottom - *top);

  const bool maximized = dictionary.FindBool("maximized").value_or(false);
  *show_state = maximized ? ui::mojom::WindowShowState::kMaximized
                          : ui::mojom::WindowShowState::kNormal;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AdjustSavedWindowPlacementChromeOS(widget, bounds);
#endif
  return true;
}

bool ChromeViewsDelegate::IsShuttingDown() const {
  return KeepAliveRegistry::GetInstance()->IsShuttingDown();
}

void ChromeViewsDelegate::AddRef() {
  if (ref_count_ == 0u) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::CHROME_VIEWS_DELEGATE,
        KeepAliveRestartOption::DISABLED);
  }

  // There's no easy way to know which Profile caused this menu to open, so
  // prevent all currently-loaded Profiles from deleting until the menu
  // closes.
  //
  // Do this unconditionally, not just when the ref-count becomes non-zero. That
  // way, we pick up any new profiles that have become loaded since the last
  // call to AddRef().
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    profile_keep_alives_[profile] = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kChromeViewsDelegate);
  }

  ++ref_count_;
}

void ChromeViewsDelegate::ReleaseRef() {
  DCHECK_NE(0u, ref_count_);

  if (--ref_count_ == 0u) {
    keep_alive_.reset();
    profile_keep_alives_.clear();
  }
}

void ChromeViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only for dialog widgets, if this is not going to be a transient child,
  // then we mark it as an OS system app, otherwise its transient root's app
  // type should be used.
  // `delegate->IsDialogBox()` does not work because the underlying Widget
  // does not have its widget delegate set before `OnBeforeWidgetInit`.
  if (params->delegate && params->delegate->AsDialogDelegate() &&
      !params->parent) {
    params->init_properties_container.SetProperty(
        chromeos::kAppTypeKey, chromeos::AppType::SYSTEM_APP);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // We need to determine opacity if it's not already specified.
  if (params->opacity == views::Widget::InitParams::WindowOpacity::kInferred) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::ResolveInferredOpacity(params);
#else
    params->opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
#endif
  }

  // If we already have a native_widget, we don't have to try to come
  // up with one.
  if (params->native_widget)
    return;

  if (!native_widget_factory().is_null()) {
    params->native_widget = native_widget_factory().Run(*params, delegate);
    if (params->native_widget)
      return;
  }

  params->native_widget = CreateNativeWidget(params, delegate);
}

std::string ChromeViewsDelegate::GetApplicationName() {
  return std::string(version_info::GetProductName());
}
