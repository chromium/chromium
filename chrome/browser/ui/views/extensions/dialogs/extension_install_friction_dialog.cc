// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

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

void AutoConfirmDialog(base::OnceCallback<void(bool)> callback) {
  switch (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), true));
      return;
    case extensions::ScopedTestDialogAutoConfirm::CANCEL:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
      return;
    default:
      NOTREACHED();
  }
}

class ExtensionInstallFrictionDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit ExtensionInstallFrictionDialogDelegate(
      base::OnceCallback<void(bool)> callback,
      content::WebContents* web_contents,
      Profile* profile)
      : callback_(std::move(callback)),
        original_web_contents_(web_contents->GetWeakPtr()),
        profile_(profile) {}

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

    GURL url(chrome::kCwsEnhancedSafeBrowsingLearnMoreURL);
    content::OpenURLParams params(
        url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);

    if (original_web_contents_) {
      original_web_contents_->OpenURL(params, {});
    } else {
      chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
      displayer.browser()->OpenURL(params, {});
    }

    dialog_model()->host()->Close();
  }

 private:
  base::OnceCallback<void(bool)> callback_;
  base::WeakPtr<content::WebContents> original_web_contents_;
  raw_ptr<Profile> profile_;
  bool learn_more_clicked_ = false;
};

}  // namespace

namespace extensions {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kExtensionInstallFrictionLearnMoreLink);

void ShowExtensionInstallFrictionDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  if (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      extensions::ScopedTestDialogAutoConfirm::NONE) {
    AutoConfirmDialog(std::move(callback));
    return;
  }

  auto dialog_delegate_unique =
      std::make_unique<ExtensionInstallFrictionDialogDelegate>(
          std::move(callback), web_contents,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
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

  constrained_window::ShowBrowserModal(std::move(dialog),
                                       web_contents->GetTopLevelNativeWindow());
}

}  // namespace extensions
