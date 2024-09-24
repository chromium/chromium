// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_POST_SAVE_COMPROMISED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_POST_SAVE_COMPROMISED_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/post_save_compromised_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Bubble notifying the user about remaining compromised credentials in the
// store.
class PostSaveCompromisedBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(PostSaveCompromisedBubbleView, PasswordBubbleViewBase)

 public:
  explicit PostSaveCompromisedBubbleView(content::WebContents* web_contents,
                                         views::View* anchor_view);
  ~PostSaveCompromisedBubbleView() override;

 private:
  // PasswordBubbleViewBase:
  PostSaveCompromisedBubbleController* GetController() override;
  const PostSaveCompromisedBubbleController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  // View:
  void AddedToWidget() override;

  PostSaveCompromisedBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_POST_SAVE_COMPROMISED_BUBBLE_VIEW_H_
