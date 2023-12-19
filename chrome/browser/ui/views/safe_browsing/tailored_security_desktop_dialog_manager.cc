// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog_manager.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"

namespace safe_browsing {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBodyText);
}  // namespace

// Base model delegate for the Tailored Security dialogs. This class implements
// the click behavior for the dialogs.
class TailoredSecurityDialogModelDelegate : public ui::DialogModelDelegate {
 public:
  explicit TailoredSecurityDialogModelDelegate(
      const char* kOutcomeMetricName,
      base::UserMetricsAction accept_user_action,
      base::UserMetricsAction settings_user_action)
      : kOutcomeMetricName_(kOutcomeMetricName),
        accept_user_action_(accept_user_action),
        settings_user_action_(settings_user_action) {}

  void OnDialogAccepted() {
    // Just count the click.
    base::UmaHistogramEnumeration(kOutcomeMetricName_,
                                  TailoredSecurityOutcome::kAccepted);
    base::RecordAction(accept_user_action_);
  }

  void OnDialogRejected(Browser* browser) {
    // Redirect to the Chrome safe browsing settings page.
    base::UmaHistogramEnumeration(kOutcomeMetricName_,
                                  TailoredSecurityOutcome::kSettings);
    base::RecordAction(settings_user_action_);

    chrome::ShowSafeBrowsingEnhancedProtection(browser);
  }

  void CloseDialog() {
    if (this->dialog_model() && this->dialog_model()->host()) {
      base::UmaHistogramEnumeration(
          kOutcomeMetricName_, TailoredSecurityOutcome::kClosedByAnotherDialog);
      this->dialog_model()->host()->Close();
    }
  }

  base::OnceCallback<void()> GetCloseDialogCallback() {
    return base::BindOnce(&TailoredSecurityDialogModelDelegate::CloseDialog,
                          weak_factory_.GetWeakPtr());
  }

 private:
  const std::string kOutcomeMetricName_;
  const base::UserMetricsAction accept_user_action_;
  const base::UserMetricsAction settings_user_action_;
  base::WeakPtrFactory<TailoredSecurityDialogModelDelegate> weak_factory_{this};
};

class DisabledDialogModelDelegate : public TailoredSecurityDialogModelDelegate {
 public:
  DisabledDialogModelDelegate()
      : TailoredSecurityDialogModelDelegate(
            kDisabledDialogOutcome,
            base::UserMetricsAction(
                safe_browsing::kTailoredSecurityDisabledDialogOkButtonClicked),
            base::UserMetricsAction(
                safe_browsing::
                    kTailoredSecurityDisabledDialogSettingsButtonClicked)) {}
};

class EnabledDialogModelDelegate : public TailoredSecurityDialogModelDelegate {
 public:
  EnabledDialogModelDelegate()
      : TailoredSecurityDialogModelDelegate(
            kEnabledDialogOutcome,
            base::UserMetricsAction(
                safe_browsing::kTailoredSecurityEnabledDialogOkButtonClicked),
            base::UserMetricsAction(
                safe_browsing::
                    kTailoredSecurityEnabledDialogSettingsButtonClicked)) {}
};

TailoredSecurityDesktopDialogManager::TailoredSecurityDesktopDialogManager() =
    default;
TailoredSecurityDesktopDialogManager::~TailoredSecurityDesktopDialogManager() =
    default;

void TailoredSecurityDesktopDialogManager::ShowEnabledDialogForBrowser(
    Browser* browser) {
  auto model_delegate = std::make_unique<EnabledDialogModelDelegate>();
  auto* model_delegate_ptr = model_delegate.get();

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  auto banner_image_light = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_CONSENTED));
  auto banner_image_dark = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_TAILORED_SECURITY_CONSENTED_DARK));

  auto body_text = ui::DialogModelLabel(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_ENABLED_DIALOG_MAIN_TEXT));
  body_text.set_is_secondary();
  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_TAILORED_SECURITY_ENABLED_DIALOG_TITLE))
          .SetInternalName(kTailoredSecurityNoticeDialog)
          .SetBannerImage(std::move(banner_image_light),
                          std::move(banner_image_dark))
          .AddParagraph(body_text, std::u16string(), kBodyText)
          .AddOkButton(
              base::BindOnce(&EnabledDialogModelDelegate::OnDialogAccepted,
                             base::Unretained(model_delegate_ptr)))
          .AddCancelButton(
              base::BindOnce(&EnabledDialogModelDelegate::OnDialogRejected,
                             base::Unretained(model_delegate_ptr), browser),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_TAILORED_SECURITY_DIALOG_SETTINGS_BUTTON)))
          .Build();

  // `window` should always be non-null unless this is called before
  // CreateBrowserWindow().
  DCHECK(browser->window());

  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
  close_dialog_callback_ = model_delegate_ptr->GetCloseDialogCallback();
  base::RecordAction(base::UserMetricsAction(
      safe_browsing::kTailoredSecurityEnabledDialogShown));
  constrained_window::ShowBrowserModal(std::move(dialog_model),
                                       browser->window()->GetNativeWindow());
}

void TailoredSecurityDesktopDialogManager::ShowDisabledDialogForBrowser(
    Browser* browser) {
  auto model_delegate = std::make_unique<DisabledDialogModelDelegate>();
  auto* model_delegate_ptr = model_delegate.get();

  auto body_text =
      ui::DialogModelLabel(l10n_util::GetStringUTF16(
                               IDS_TAILORED_SECURITY_DISABLED_DIALOG_MAIN_TEXT))
          .set_is_secondary();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_TAILORED_SECURITY_DISABLED_DIALOG_TITLE))
          .SetInternalName(kTailoredSecurityNoticeDialog)
          .AddParagraph(body_text, std::u16string(), kBodyText)
          .AddOkButton(
              base::BindOnce(&DisabledDialogModelDelegate::OnDialogAccepted,
                             base::Unretained(model_delegate_ptr)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_TAILORED_SECURITY_DISABLED_DIALOG_ACCEPT_BUTTON)))
          .AddCancelButton(
              base::BindOnce(&DisabledDialogModelDelegate::OnDialogRejected,
                             base::Unretained(model_delegate_ptr), browser),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_TAILORED_SECURITY_DIALOG_SETTINGS_BUTTON)))
          .Build();

  // `window` should always be non-null unless this is called before
  // CreateBrowserWindow().
  DCHECK(browser->window());

  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
  close_dialog_callback_ = model_delegate_ptr->GetCloseDialogCallback();
  base::RecordAction(base::UserMetricsAction(
      safe_browsing::kTailoredSecurityDisabledDialogShown));
  constrained_window::ShowBrowserModal(std::move(dialog_model),
                                       browser->window()->GetNativeWindow());
}

}  // namespace safe_browsing
