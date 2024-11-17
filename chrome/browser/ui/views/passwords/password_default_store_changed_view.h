// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_DEFAULT_STORE_CHANGED_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_DEFAULT_STORE_CHANGED_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/default_store_changed_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

// A view informing the user that their setting for a default password store was
// changed, offering a link to the password manager settings page.
class PasswordDefaultStoreChangedView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordDefaultStoreChangedView, PasswordBubbleViewBase)

 public:
  PasswordDefaultStoreChangedView(content::WebContents* web_contents,
                                  views::View* anchor_view);
  ~PasswordDefaultStoreChangedView() override;

 private:
  // PasswordBubbleViewBase:
  DefaultStoreChangedBubbleController* GetController() override;
  const DefaultStoreChangedBubbleController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  DefaultStoreChangedBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_DEFAULT_STORE_CHANGED_VIEW_H_
