// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TASKBAR_ICON_UPDATER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TASKBAR_ICON_UPDATER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

// Watches for profile changes and updates the window icon.
class WindowsTaskbarIconUpdater : public ProfileAttributesStorage::Observer,
                                  public views::ViewObserver,
                                  public views::WidgetObserver {
 public:
  explicit WindowsTaskbarIconUpdater(BrowserView& browser_view);
  ~WindowsTaskbarIconUpdater() override;

 private:
  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

  // ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;

  // WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Updates the taskbar icon for `browser_view_`'s window.
  void UpdateIcon();

  raw_ref<BrowserView> browser_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observer_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observer_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TASKBAR_ICON_UPDATER_H_
