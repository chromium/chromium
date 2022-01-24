// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/tailored_security_unconsented_modal.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace safe_browsing {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ModalOutcome {
  MODAL_OUTCOME_ACCEPT = 0,
  MODAL_OUTCOME_CANCEL = 1,
  MODAL_OUTCOME_CLOSE = 2,
  kMaxValue = MODAL_OUTCOME_CLOSE,
};

void RecordModalOutcomeAndRunCallback(ModalOutcome outcome,
                                      base::OnceClosure callback) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurityUnconsentedModalOutcome", outcome);
  std::move(callback).Run();
}

void EnableEsbAndShowSettings(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SetSafeBrowsingState(profile->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  if (!chrome::FindBrowserWithWebContents(web_contents))
    return;
  chrome::ShowSafeBrowsingEnhancedProtection(
      chrome::FindBrowserWithWebContents(web_contents));
}

}  // namespace

/*static*/
void TailoredSecurityUnconsentedModal::ShowForWebContents(
    content::WebContents* web_contents) {
  constrained_window::ShowWebModalDialogViews(
      new TailoredSecurityUnconsentedModal(web_contents), web_contents);
}

TailoredSecurityUnconsentedModal::TailoredSecurityUnconsentedModal(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  SetModalType(ui::MODAL_TYPE_CHILD);

  SetTitle(IDS_TAILORED_SECURITY_UNCONSENTED_MODAL_TITLE);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_TAILORED_SECURITY_UNCONSENTED_ACCEPT_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_TAILORED_SECURITY_UNCONSENTED_CANCEL_BUTTON));

  SetAcceptCallback(base::BindOnce(
      RecordModalOutcomeAndRunCallback, ModalOutcome::MODAL_OUTCOME_ACCEPT,
      base::BindOnce(&EnableEsbAndShowSettings, web_contents_)));
  SetCancelCallback(base::BindOnce(RecordModalOutcomeAndRunCallback,
                                   ModalOutcome::MODAL_OUTCOME_CANCEL,
                                   base::DoNothing()));
  SetCloseCallback(base::BindOnce(RecordModalOutcomeAndRunCallback,
                                  ModalOutcome::MODAL_OUTCOME_CLOSE,
                                  base::DoNothing()));
}

TailoredSecurityUnconsentedModal::~TailoredSecurityUnconsentedModal() = default;

bool TailoredSecurityUnconsentedModal::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
}

bool TailoredSecurityUnconsentedModal::ShouldShowCloseButton() const {
  return false;
}

BEGIN_METADATA(TailoredSecurityUnconsentedModal, views::DialogDelegateView)
END_METADATA

}  // namespace safe_browsing
