// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"

#include <string>

#include "chrome/browser/ui/digital_credentials/digital_identity_safety_interstitial_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"

using DialogButton = ui::DialogModel::Button;
using InterstitialType = content::DigitalIdentityInterstitialType;

DigitalIdentitySafetyInterstitialControllerDesktop::
    DigitalIdentitySafetyInterstitialControllerDesktop() = default;
DigitalIdentitySafetyInterstitialControllerDesktop::
    ~DigitalIdentitySafetyInterstitialControllerDesktop() = default;

content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback
DigitalIdentitySafetyInterstitialControllerDesktop::ShowInterstitial(
    content::WebContents& web_contents,
    const url::Origin& rp_origin,
    InterstitialType interstitial_type,
    content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
        callback) {
  web_contents_ = web_contents.GetWeakPtr();
  rp_origin_ = rp_origin;
  interstitial_type_ = interstitial_type;
  callback_ = std::move(callback);

  ShowInterstitialImpl(web_contents, /*was_request_aborted=*/false);
  return base::BindOnce(
      &DigitalIdentitySafetyInterstitialControllerDesktop::Abort,
      weak_ptr_factory_.GetWeakPtr());
}

void DigitalIdentitySafetyInterstitialControllerDesktop::Abort() {
  if (!web_contents_) {
    return;
  }

  dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  ShowInterstitialImpl(*web_contents_, /*was_request_aborted*/ true);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::ShowInterstitialImpl(
    content::WebContents& web_contents,
    bool was_request_aborted) {
  int body_resource_id = 0;
  int negative_button_label_resource_id = 0;
  switch (interstitial_type_) {
    case InterstitialType::kHighRisk:
      body_resource_id =
          IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_HIGH_RISK_DIALOG_TEXT;
      negative_button_label_resource_id =
          IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_HIGH_RISK_NEGATIVE_BUTTON_TEXT;
      break;
    case InterstitialType::kLowRisk:
      body_resource_id =
          IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_LOW_RISK_DIALOG_TEXT;
      negative_button_label_resource_id =
          IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_LOW_RISK_NEGATIVE_BUTTON_TEXT;
      break;
  }

  bool positive_button_enabled = !was_request_aborted;

  std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          rp_origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  std::u16string body_text =
      l10n_util::GetStringFUTF16(body_resource_id, formatted_origin);
  std::u16string positive_button_label = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_POSITIVE_BUTTON_TEXT);
  std::u16string negative_button_label =
      l10n_util::GetStringUTF16(negative_button_label_resource_id);

  ui::DialogModel::Builder dialog_model_builder(
      std::make_unique<ui::DialogModelDelegate>());
  dialog_model_builder
      .AddOkButton(
          base::BindOnce(&DigitalIdentitySafetyInterstitialControllerDesktop::
                             OnUserGrantedPermission,
                         weak_ptr_factory_.GetWeakPtr()),
          DialogButton::Params()
              .SetLabel(positive_button_label)
              .SetStyle(ui::ButtonStyle::kText)
              .SetEnabled(positive_button_enabled))
      .AddCancelButton(
          base::BindOnce(&DigitalIdentitySafetyInterstitialControllerDesktop::
                             OnUserDeniedPermission,
                         weak_ptr_factory_.GetWeakPtr()),
          DialogButton::Params()
              .SetLabel(negative_button_label)
              .SetStyle(ui::ButtonStyle::kProminent))
      .OverrideDefaultButton(ui::DIALOG_BUTTON_CANCEL)
      .SetDialogDestroyingCallback(
          base::BindOnce(&DigitalIdentitySafetyInterstitialControllerDesktop::
                             OnUserDeniedPermission,
                         weak_ptr_factory_.GetWeakPtr()))
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE))
      .AddParagraph(ui::DialogModelLabel(body_text));

  if (was_request_aborted) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel(l10n_util::GetStringFUTF16(
            IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_REQUEST_ABORTED_DIALOG_TEXT,
            formatted_origin)));
  }
  dialog_widget_ = constrained_window::ShowWebModal(
      dialog_model_builder.Build(), &web_contents);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::
    OnUserDeniedPermission() {
  if (!callback_) {
    return;
  }

  std::move(callback_).Run(
      content::DigitalIdentityProvider::RequestStatusForMetrics::kErrorOther);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::
    OnUserGrantedPermission() {
  if (!callback_) {
    return;
  }

  std::move(callback_).Run(
      content::DigitalIdentityProvider::RequestStatusForMetrics::kSuccess);
}
