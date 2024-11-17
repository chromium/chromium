// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/save_unsynced_credentials_locally_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/view.h"

// A dialog that shows up on sign out in case there are passwords not committed
// to the user account. By clicking the save button, the user can save those
// passwords locally.
class PasswordSaveUnsyncedCredentialsLocallyView
    : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordSaveUnsyncedCredentialsLocallyView,
                  PasswordBubbleViewBase)

 public:
  PasswordSaveUnsyncedCredentialsLocallyView(content::WebContents* web_contents,
                                             views::View* anchor_view);
  ~PasswordSaveUnsyncedCredentialsLocallyView() override;

 private:
  // PasswordBubbleViewBase:
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  void CreateLayout();

  void ButtonPressed(views::Checkbox* checkbox);

  void OnSaveClicked();

  SaveUnsyncedCredentialsLocallyBubbleController controller_;
  int num_selected_checkboxes_ = 0;
  std::vector<raw_ptr<views::Checkbox, VectorExperimental>> checkboxes_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UNSYNCED_CREDENTIALS_LOCALLY_VIEW_H_
