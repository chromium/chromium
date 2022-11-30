// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_widget_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"

views::NativeWidget* CreateNativeWidget(
    NativeWidgetType type,
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // While the majority of the time, context wasn't plumbed through due to the
  // existence of a global WindowParentingClient, if this window is toplevel,
  // it's possible that there is no contextual state that we can use.
  gfx::NativeWindow parent_or_context =
      params->parent ? params->parent : params->context;
  Profile* profile = nullptr;
  if (parent_or_context)
    profile = GetThemeProfileForWindow(parent_or_context);
  views::NativeWidget* native_widget = nullptr;
  aura::Window* window = nullptr;
  if (type == NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA ||
      (!params->parent && !params->context && !params->child)) {
    // In the desktop case, do not always set the profile window
    // property from the parent since there are windows (like the task
    // manager) that are not associated with a specific profile.
    views::DesktopNativeWidgetAura* desktop_native_widget =
        new views::DesktopNativeWidgetAura(delegate);
    window = desktop_native_widget->GetNativeWindow();
    native_widget = desktop_native_widget;
  } else {
    views::NativeWidgetAura* native_widget_aura =
        new views::NativeWidgetAura(delegate);
    if (params->parent) {
      Profile* parent_profile = reinterpret_cast<Profile*>(
          params->parent->GetNativeWindowProperty(Profile::kProfileKey));
      native_widget_aura->SetNativeWindowProperty(Profile::kProfileKey,
                                                  parent_profile);
    }
    window = native_widget_aura->GetNativeWindow();
    native_widget = native_widget_aura;
  }
  // Use the original profile because |window| may outlive the profile
  // of the context window.  This can happen with incognito profiles.
  // However, the original profile will stick around until shutdown.
  SetThemeProfileForWindow(window,
                           profile ? profile->GetOriginalProfile() : nullptr);
  return native_widget;
}
