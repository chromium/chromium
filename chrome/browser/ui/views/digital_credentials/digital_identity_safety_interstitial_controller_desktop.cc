// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "chrome/browser/digital_credentials/digital_identity_interstitial_closed_reason.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/widget/widget.h"

using DialogButton = ui::DialogModel::Button;
using InterstitialType = content::DigitalIdentityInterstitialType;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using web_modal::WebContentsModalDialogManager;

DigitalIdentitySafetyInterstitialControllerDesktop::CloseOnNavigationObserver::
    CloseOnNavigationObserver() = default;

DigitalIdentitySafetyInterstitialControllerDesktop::CloseOnNavigationObserver::
    ~CloseOnNavigationObserver() {
  if (!web_contents_) {
    return;
  }
  WebContentsModalDialogManager::FromWebContents(web_contents_.get())
      ->RemoveCloseOnNavigationObserver(this);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::
    CloseOnNavigationObserver::Observe(content::WebContents& web_contents) {
  web_contents_ = web_contents.GetWeakPtr();
  WebContentsModalDialogManager::FromWebContents(web_contents_.get())
      ->AddCloseOnNavigationObserver(this);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::
    CloseOnNavigationObserver::OnWillClose() {
  will_close_due_to_navigation_ = true;
}

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
                             OnDialogClosed,
                         weak_ptr_factory_.GetWeakPtr(),
                         DigitalIdentityInterstitialClosedReason::kOkButton),
          DialogButton::Params()
              .SetLabel(positive_button_label)
              .SetStyle(ui::ButtonStyle::kText)
              .SetEnabled(positive_button_enabled))
      .AddCancelButton(
          base::BindOnce(
              &DigitalIdentitySafetyInterstitialControllerDesktop::
                  OnDialogClosed,
              weak_ptr_factory_.GetWeakPtr(),
              DigitalIdentityInterstitialClosedReason::kCancelButton),
          DialogButton::Params()
              .SetLabel(negative_button_label)
              .SetStyle(ui::ButtonStyle::kProminent))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .SetDialogDestroyingCallback(base::BindOnce(
          &DigitalIdentitySafetyInterstitialControllerDesktop::OnDialogClosed,
          weak_ptr_factory_.GetWeakPtr(),
          DigitalIdentityInterstitialClosedReason::kOther))
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

  close_on_navigation_observer_ = std::make_unique<CloseOnNavigationObserver>();
  close_on_navigation_observer_->Observe(web_contents);
}

void DigitalIdentitySafetyInterstitialControllerDesktop::OnDialogClosed(
    DigitalIdentityInterstitialClosedReason closed_reason) {
  if (!callback_) {
    return;
  }

  if (close_on_navigation_observer_ &&
      close_on_navigation_observer_->WillCloseOnNavigation()) {
    closed_reason = DigitalIdentityInterstitialClosedReason::kPageNavigated;
  }

  base::UmaHistogramEnumeration(
      "Blink.DigitalIdentityRequest.InterstitialClosedReason", closed_reason);

  std::move(callback_).Run(
      closed_reason == DigitalIdentityInterstitialClosedReason::kOkButton
          ? RequestStatusForMetrics::kSuccess
          : RequestStatusForMetrics::kErrorOther);
}
