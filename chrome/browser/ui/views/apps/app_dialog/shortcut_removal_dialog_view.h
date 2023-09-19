// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_SHORTCUT_REMOVAL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_SHORTCUT_REMOVAL_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/shortcut_removal_dialog.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace gfx {
class ImageSkia;
}

// This class generates the unified shortcut removal dialog.
class ShortcutRemovalDialogView : public ProfileObserver, public AppDialogView {
 public:
  ShortcutRemovalDialogView(
      Profile* profile,
      const apps::ShortcutId& shortcut_id,
      gfx::ImageSkia icon_with_badge,
      base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog);

  ShortcutRemovalDialogView(const ShortcutRemovalDialogView&) = delete;
  ShortcutRemovalDialogView& operator=(const ShortcutRemovalDialogView&) =
      delete;

  ~ShortcutRemovalDialogView() override;

  static ShortcutRemovalDialogView* GetLastCreatedViewForTesting();

  base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog() const {
    return shortcut_removal_dialog_;
  }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void InitializeView(Profile* profile, const apps::ShortcutId& shortcut_id);

  void OnDialogCancelled();
  void OnDialogAccepted();

  raw_ptr<Profile> profile_;

  base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<ShortcutRemovalDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_SHORTCUT_REMOVAL_DIALOG_VIEW_H_
