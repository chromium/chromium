// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_id.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExtensionInstallFrictionDialogAction {
  kClose = 0,
  kLearnMore = 1,
  kContinueToInstall = 2,
  kClosedWithoutUserAction = 3,
  kMaxValue = kClosedWithoutUserAction,
};

void ReportExtensionInstallFrictionDialogAction(
    ExtensionInstallFrictionDialogAction action) {
  base::UmaHistogramEnumeration("Extensions.InstallFrictionDialogAction",
                                action);
}

class ExtensionInstallFrictionDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit ExtensionInstallFrictionDialogDelegate(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> callback)
      : original_web_contents_(web_contents->GetWeakPtr()),
        callback_(std::move(callback)) {}

  ~ExtensionInstallFrictionDialogDelegate() override = default;

  void OnDialogAccepted() {
    ReportExtensionInstallFrictionDialogAction(
        ExtensionInstallFrictionDialogAction::kContinueToInstall);
    std::move(callback_).Run(true);
  }

  void OnDialogCanceled() {
    ExtensionInstallFrictionDialogAction dialog_action =
        learn_more_clicked_ ? ExtensionInstallFrictionDialogAction::kLearnMore
                            : ExtensionInstallFrictionDialogAction::kClose;
    ReportExtensionInstallFrictionDialogAction(dialog_action);
    std::move(callback_).Run(false);
  }

  void OnDialogDestroyed() {
    if (callback_) {
      // The dialog may close without firing any of the [accept | cancel |
      // close] callbacks if e.g. the parent window closes. In this case, we
      // have to manually run the callback.
      ReportExtensionInstallFrictionDialogAction(
          ExtensionInstallFrictionDialogAction::kClosedWithoutUserAction);
      std::move(callback_).Run(false);
    }
  }

  void OnLearnMoreLinkClicked() {
    learn_more_clicked_ = true;

    if (original_web_contents_) {
      GURL url(chrome::kCwsEnhancedSafeBrowsingLearnMoreURL);
      content::OpenURLParams params(
          url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
      original_web_contents_->OpenURL(params, {});
    }

    dialog_model()->host()->Close();
  }

 private:
  base::WeakPtr<content::WebContents> original_web_contents_;
  base::OnceCallback<void(bool)> callback_;
  bool learn_more_clicked_ = false;
};

}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kExtensionInstallFrictionLearnMoreLink);

void ShowExtensionInstallFrictionDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  auto dialog_delegate_unique =
      std::make_unique<ExtensionInstallFrictionDialogDelegate>(
          web_contents, std::move(callback));
  ExtensionInstallFrictionDialogDelegate* dialog_delegate =
      dialog_delegate_unique.get();

  std::unique_ptr<ui::DialogModel> dialog =
      ui::DialogModel::Builder(std::move(dialog_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_INSTALL_FRICTION_TITLE))
          .SetIcon(ui::ImageModel::FromVectorIcon(
              vector_icons::kGppMaybeIcon, ui::kColorAlertMediumSeverityIcon,
              extension_misc::EXTENSION_ICON_SMALLISH))
          .AddOkButton(
              base::BindOnce(
                  &ExtensionInstallFrictionDialogDelegate::OnDialogAccepted,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSION_PROMPT_INSTALL_FRICTION_CONTINUE_BUTTON)))
          .AddCancelButton(
              base::BindOnce(
                  &ExtensionInstallFrictionDialogDelegate::OnDialogCanceled,
                  base::Unretained(dialog_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(IDS_CLOSE)))
          .SetCloseActionCallback(base::BindOnce(
              &ExtensionInstallFrictionDialogDelegate::OnDialogCanceled,
              base::Unretained(dialog_delegate)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &ExtensionInstallFrictionDialogDelegate::OnDialogDestroyed,
              base::Unretained(dialog_delegate)))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
                            IDS_EXTENSION_PROMPT_INSTALL_FRICTION_WARNING_TEXT,
                            ui::DialogModelLabel::CreateLink(
                                IDS_LEARN_MORE,
                                base::BindRepeating(
                                    &ExtensionInstallFrictionDialogDelegate::
                                        OnLearnMoreLinkClicked,
                                    base::Unretained(dialog_delegate)))),
                        /*header=*/std::u16string(),
                        /*id=*/kExtensionInstallFrictionLearnMoreLink)
          .Build();

  ShowWebModalDialog(web_contents, std::move(dialog));
}

}  // namespace extensions
