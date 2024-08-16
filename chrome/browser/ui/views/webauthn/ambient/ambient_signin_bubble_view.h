// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

struct AuthenticatorRequestDialogModel;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Label;
}  // namespace views

namespace ambient_signin {

class AmbientSigninController;

class AmbientSigninBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(AmbientSigninBubbleView, views::BubbleDialogDelegateView)
 public:
  explicit AmbientSigninBubbleView(content::WebContents* web_contents,
                                   views::View* anchor_view,
                                   AmbientSigninController* controller,
                                   AuthenticatorRequestDialogModel* model);
  ~AmbientSigninBubbleView() override;

  void Show();
  void Update();
  void Hide();
  void Close();

  // views::BubbleDialogDelegateView:
  void NotifyWidgetDestroyed();

 private:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;

  std::vector<std::unique_ptr<views::Label>> labels_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<AmbientSigninController> controller_;
  base::WeakPtr<views::Widget> widget_;
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_
