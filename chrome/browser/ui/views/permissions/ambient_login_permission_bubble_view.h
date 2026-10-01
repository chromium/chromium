// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace ui {
class Event;
}

namespace views {
class MdTextButton;
}

class AmbientLoginPermissionBubbleView : public PermissionPromptBubbleBaseView {
  METADATA_HEADER(AmbientLoginPermissionBubbleView,
                  PermissionPromptBubbleBaseView)

 public:
  AmbientLoginPermissionBubbleView(
      content::WebContents* web_contents,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      PermissionPromptStyle prompt_style);
  AmbientLoginPermissionBubbleView(const AmbientLoginPermissionBubbleView&) =
      delete;
  AmbientLoginPermissionBubbleView& operator=(
      const AmbientLoginPermissionBubbleView&) = delete;
  ~AmbientLoginPermissionBubbleView() override;

  // PermissionPromptBubbleBaseView:
  void Show() override;
  void RunButtonCallback(int button_id) override;

  // views::View:
  void AddedToWidget() override;

 private:
  void OnSignInButtonClicked(const ui::Event& event);

  raw_ptr<views::MdTextButton> sign_in_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_
