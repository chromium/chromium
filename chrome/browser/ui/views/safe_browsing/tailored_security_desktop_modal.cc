// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_modal.h"

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_outcome.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"

namespace safe_browsing {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBodyText);

// Model delegate for the disabled modal. This class implements the click
// behavior for the disabled modal.
class DisabledModalModelDelegate : public ui::DialogModelDelegate {
 public:
  void OnDialogAccepted() {
    // Just count the click.
    base::UmaHistogramEnumeration(kModalDisabledOutcome,
                                  TailoredSecurityOutcome::kAccepted);
  }
  void OnDialogRejected(content::WebContents* web_contents) {
    // Redirect to the Chrome safe browsing settings page.
    base::UmaHistogramEnumeration(kModalDisabledOutcome,
                                  TailoredSecurityOutcome::kSettings);

    chrome::ShowSafeBrowsingEnhancedProtection(
        chrome::FindBrowserWithWebContents(web_contents));
  }
};

// Model delegate for the enabled modal. This class implements the click
// behavior for the enabled modal.
class EnabledModalModelDelegate : public ui::DialogModelDelegate {
 public:
  void OnDialogAccepted() {
    // Just count the click.
    base::UmaHistogramEnumeration(kModalEnabledOutcome,
                                  TailoredSecurityOutcome::kAccepted);
  }
  void OnDialogRejected(content::WebContents* web_contents) {
    // Redirect to the Chrome safe browsing settings page.
    base::UmaHistogramEnumeration(kModalEnabledOutcome,
                                  TailoredSecurityOutcome::kSettings);

    chrome::ShowSafeBrowsingEnhancedProtection(
        chrome::FindBrowserWithWebContents(web_contents));
  }
};

void ShowEnabledModalForWebContents(content::WebContents* web_contents) {
  auto model_delegate = std::make_unique<EnabledModalModelDelegate>();
  auto* model_delegate_ptr = model_delegate.get();

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  auto banner_image_light = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_CONSENTED));
  auto banner_image_dark = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_CONSENTED_DARK));

  auto body_text = ui::DialogModelLabel(
      l10n_util::GetStringUTF16(IDS_TAILORED_SECURITY_ENABLED_MODAL_MAIN_TEXT));
  body_text.set_is_secondary();
  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_TAILORED_SECURITY_ENABLED_MODAL_TITLE))
          .SetInternalName(kTailoredSecurityNoticeModal)
          .SetBannerImage(std::move(banner_image_light),
                          std::move(banner_image_dark))
          .AddBodyText(body_text, kBodyText)
          .AddOkButton(
              base::BindOnce(&EnabledModalModelDelegate::OnDialogAccepted,
                             base::Unretained(model_delegate_ptr)))
          .AddCancelButton(
              base::BindOnce(&EnabledModalModelDelegate::OnDialogRejected,
                             base::Unretained(model_delegate_ptr),
                             web_contents),
              l10n_util::GetStringUTF16(
                  IDS_TAILORED_SECURITY_MODAL_SETTINGS_BUTTON))
          .Build();

  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}

void ShowDisabledModalForWebContents(content::WebContents* web_contents) {
  auto model_delegate = std::make_unique<DisabledModalModelDelegate>();
  auto* model_delegate_ptr = model_delegate.get();

  auto body_text =
      ui::DialogModelLabel(l10n_util::GetStringUTF16(
                               IDS_TAILORED_SECURITY_DISABLED_MODAL_MAIN_TEXT))
          .set_is_secondary();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_TAILORED_SECURITY_DISABLED_MODAL_TITLE))
          .SetInternalName(kTailoredSecurityNoticeModal)
          .AddBodyText(body_text, kBodyText)
          .AddOkButton(
              base::BindOnce(&DisabledModalModelDelegate::OnDialogAccepted,
                             base::Unretained(model_delegate_ptr)))
          .AddCancelButton(
              base::BindOnce(&DisabledModalModelDelegate::OnDialogRejected,
                             base::Unretained(model_delegate_ptr),
                             web_contents),
              l10n_util::GetStringUTF16(
                  IDS_TAILORED_SECURITY_MODAL_SETTINGS_BUTTON))
          .Build();

  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}
}  // namespace safe_browsing
