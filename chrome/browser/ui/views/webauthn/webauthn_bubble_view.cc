// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/webauthn_bubble_view.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/account_hover_list_model.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"

// static
WebAuthnBubbleView* WebAuthnBubbleView::Create(
    const std::string& relying_party_id,
    std::vector<device::PublicKeyCredentialUserEntity> users,
    SelectedCallback selected_callback,
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto bubble_view = std::make_unique<WebAuthnBubbleView>(
      relying_party_id, std::move(users), std::move(selected_callback),
      button_provider->GetAnchorView(PageActionIconType::kWebAuthn),
      web_contents);
  WebAuthnBubbleView* weak_bubble_view = bubble_view.get();
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  bubble_widget->Show();
  return weak_bubble_view;
}

WebAuthnBubbleView::WebAuthnBubbleView(
    const std::string& relying_party_id,
    std::vector<device::PublicKeyCredentialUserEntity> users,
    SelectedCallback selected_callback,
    views::View* anchor_view,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      relying_party_id_(relying_party_id),
      users_(std::move(users)),
      selected_callback_(std::move(selected_callback)) {
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_CLOSE));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

WebAuthnBubbleView::~WebAuthnBubbleView() = default;

std::u16string WebAuthnBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/1179014): go through ux review and i18n this string.
  return base::UTF8ToUTF16(users_.empty() ? "Sign in with your security key"
                                          : "Choose an account to sign in");
}

void WebAuthnBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (users_.empty()) {
    // TODO(crbug.com/1179014): go through ux review and i18n this string.
    std::u16string label_text = base::ReplaceStringPlaceholders(
        base::UTF8ToUTF16(
            "To sign in to $1 with your security key, insert it and tap it"),
        webauthn_ui_helpers::RpIdToElidedHost(relying_party_id_, fixed_width()),
        /*offset=*/nullptr);
    auto label = std::make_unique<views::Label>(
        label_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    AddChildView(std::move(label));
    return;
  }

  AddChildView(std::make_unique<HoverListView>(
      std::make_unique<AccountHoverListModel>(&users_, this)));
}

void WebAuthnBubbleView::OnItemSelected(int index) {
  std::move(selected_callback_).Run(index);
  CloseBubble();
}
