// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_ICON_VIEW_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"

class WebAuthnBubbleView;

// Location bar icon shown when the WebAuthn Conditional UI is invoked. Clicking
// it displays a |WebAuthnBubbleView|.
class WebAuthnIconView : public PageActionIconView,
                         public views::WidgetObserver,
                         public AuthenticatorRequestDialogModel::Observer {
 public:
  METADATA_HEADER(WebAuthnIconView);
  WebAuthnIconView(CommandUpdater* command_updater,
                   IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                   PageActionIconView::Delegate* page_action_icon_delegate);
  WebAuthnIconView(const WebAuthnIconView&) = delete;
  WebAuthnIconView& operator=(const WebAuthnIconView&) = delete;
  ~WebAuthnIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;
  void OnStepTransition() override;

 private:
  base::flat_map<content::WebContents*, AuthenticatorRequestDialogModel*>
      dialog_models_;

  // The bubble is owned by its widget.
  WebAuthnBubbleView* webauthn_bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_ICON_VIEW_H_
