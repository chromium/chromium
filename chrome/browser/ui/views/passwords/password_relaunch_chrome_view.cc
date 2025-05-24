// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_relaunch_chrome_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_list_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"

RelaunchChromeView::RelaunchChromeView(content::WebContents* web_contents,
                                       views::View* anchor_view,
                                       PrefService* prefs)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents), prefs) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_.GetContinueButtonText());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_.GetNoThanksButtonText());

  SetShowIcon(true);
  SetTitle(controller_.GetTitle());

  auto label = std::make_unique<views::Label>();
  label->SetText(controller_.GetBody());
  label->SetMultiLine(/*multi_line=*/true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddChildView(std::move(label));

  SetAcceptCallback(base::BindOnce(&RelaunchChromeBubbleController::OnAccepted,
                                   base::Unretained(&controller_)));

  SetCancelCallback(base::BindOnce(&RelaunchChromeBubbleController::OnCanceled,
                                   base::Unretained(&controller_)));
}

RelaunchChromeView::~RelaunchChromeView() = default;

RelaunchChromeBubbleController* RelaunchChromeView::GetController() {
  return &controller_;
}

const RelaunchChromeBubbleController* RelaunchChromeView::GetController()
    const {
  return &controller_;
}

ui::ImageModel RelaunchChromeView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(RelaunchChromeView)
END_METADATA
