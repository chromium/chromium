// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_notice_bubble_view.h"

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/fill_layout.h"

// static
void ShowPrivacySandboxNoticeBubble(Browser* browser) {
  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetAppMenuAnchorConfiguration(browser);
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      configuration.anchor_view, configuration.bubble_arrow);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->set_close_on_deactivate(false);
  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  bubble_delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_ACKNOWLEDGE_BUTTON));
  bubble_delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_OPEN_SETTINGS_BUTTON));

  bubble_delegate->SetContentsView(
      std::make_unique<PrivacySandboxNoticeBubbleView>(browser));
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

PrivacySandboxNoticeBubbleView::PrivacySandboxNoticeBubbleView(Browser* browser)
    : browser_(browser) {
  // TODO(crbug.com/1321587): Implement view.
  SetUseDefaultFillLayout(true);
}

BEGIN_METADATA(PrivacySandboxNoticeBubbleView, views::View)
END_METADATA
