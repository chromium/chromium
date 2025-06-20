// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_taskbar_icon_updater.h"

#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/taskbar/taskbar_decorator_win.h"

WindowsTaskbarIconUpdater::WindowsTaskbarIconUpdater(BrowserView& browser_view)
    : browser_view_(browser_view) {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    profile_observation_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  } else {
    CHECK_IS_TEST();
  }

  browser_view_observer_.Observe(&*browser_view_);
}

WindowsTaskbarIconUpdater::~WindowsTaskbarIconUpdater() = default;

void WindowsTaskbarIconUpdater::OnProfileAdded(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void WindowsTaskbarIconUpdater::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  UpdateIcon();
}

void WindowsTaskbarIconUpdater::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void WindowsTaskbarIconUpdater::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void WindowsTaskbarIconUpdater::OnViewAddedToWidget(
    views::View* observed_view) {
  // When the browser view is created, it does not yet have a widget; this
  // attaches the widget when it is created.
  browser_widget_observer_.Observe(observed_view->GetWidget());
  browser_view_observer_.Reset();
}

void WindowsTaskbarIconUpdater::OnWidgetVisibilityChanged(views::Widget* widget,
                                                          bool visible) {
  // UpdateTaskbarDecoration() calls DrawTaskbarDecoration(), but that does
  // nothing if the window is not visible.  So even if we've already gotten the
  // up-to-date decoration, we need to run the update procedure again here when
  // the window becomes visible.
  if (visible) {
    UpdateIcon();
  }
}

void WindowsTaskbarIconUpdater::UpdateIcon() {
  taskbar::UpdateTaskbarDecoration(browser_view_->browser()->profile(),
                                   browser_view_->GetNativeWindow());
}
