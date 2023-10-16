// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace gfx {
class RectF;
}

// Controls how Compose dialogs are shown and hidden, and animations related to
// both actions.
// TODO(b/305019677): Refactor this class to not be a WebContentsUserData and be
// owned by ChromeComposeClient instead.
class ComposeDialogController
    : public content::WebContentsUserData<ComposeDialogController> {
 public:
  ~ComposeDialogController() override;
  ComposeDialogController(const ComposeDialogController&) = delete;
  ComposeDialogController& operator=(const ComposeDialogController&) = delete;

  static ComposeDialogController* GetOrCreate(
      content::WebContents* web_contents);

  // Create and show the dialog view.
  void ShowComposeDialog(views::View* anchor_view,
                         const gfx::RectF& element_bounds_in_screen);

  // Returns bounds of a compose dialog centered on |element_bounds_in_screen|.
  gfx::Rect ComputeCenteredDialogBoundsInScreen(
      const gfx::Size dialog_size,
      const gfx::RectF& element_bounds_in_screen);

  // Returns the currently shown compose dialog. Returns nullptr if the dialog
  // is not currently shown.
  compose::ComposeDialogView* GetComposeDialog() const;

  void CloseDialog();

 protected:
  explicit ComposeDialogController(content::WebContents* contents);

 private:
  // Unowned reference for compose dialog. This will be nulptr if no dialog is
  // currently shown.
  // TODO(b/300939629): Pass callback to dialog view construction to null out
  // this pointer when the dialog is closed.
  raw_ptr<compose::ComposeDialogView> compose_dialog_view_ = nullptr;

  friend class content::WebContentsUserData<ComposeDialogController>;
  friend class ComposeDialogControllerTest;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ComposeDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_CONTROLLER_H_
