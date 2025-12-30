// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_post_install_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/extensions/extension_post_install_dialog_delegate.h"
#endif

namespace extensions {

namespace {
constexpr gfx::Size kMaxIconSize{43, 43};

void ConfigurePostInstallDialogModel(
    ui::DialogModel::Builder& dialog_model_builder,
    ExtensionPostInstallDialogModel* model,
    base::RepeatingClosure manage_shortcuts_callback) {
  std::u16string extension_name =
      extensions::util::GetFixupExtensionNameForUIDisplay(
          model->extension_name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  dialog_model_builder
      .SetTitle(l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                           extension_name))
      .SetIcon(
          ui::ImageModel::FromImageSkia(model->MakeIconOfSize(kMaxIconSize)));

  if (model->show_how_to_use()) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel(model->GetHowToUseText()));
  }
  if (model->show_key_binding() && manage_shortcuts_callback) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel::CreateWithReplacement(
            IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS,
            ui::DialogModelLabel::CreateLink(
                IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS_LINK_TEXT,
                manage_shortcuts_callback)));
  }
  if (model->show_how_to_manage()) {
    dialog_model_builder.AddParagraph(ui::DialogModelLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
}

void OpenExtensionsShortcutsPage(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }
  const GURL kUrl(base::StrCat({chrome::kChromeUIExtensionsURL,
                                chrome::kExtensionConfigureCommandsSubPage}));
  content::OpenURLParams params(
      kUrl, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  web_contents->OpenURL(params, {});
}

class ExtensionPostInstallDialog : public ui::DialogModelDelegate {
 public:
  ExtensionPostInstallDialog(
      content::WebContents* web_contents,
      std::unique_ptr<ExtensionPostInstallDialogModel> model)
      : web_contents_(web_contents->GetWeakPtr()), model_(std::move(model)) {}

  ExtensionPostInstallDialog(const ExtensionPostInstallDialog&) = delete;
  ExtensionPostInstallDialog& operator=(const ExtensionPostInstallDialog&) =
      delete;
  ~ExtensionPostInstallDialog() override = default;

  ExtensionPostInstallDialogModel* model() { return model_.get(); }

  void LinkClicked() {
    extensions::OpenExtensionsShortcutsPage(web_contents_);
    dialog_model()->host()->Close();
  }

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  const std::unique_ptr<ExtensionPostInstallDialogModel> model_;
};

}  // namespace

void ShowExtensionPostInstallDialog(
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<ExtensionPostInstallDialogModel> model) {
  if (!web_contents) {
    return;
  }

  auto delegate = std::make_unique<ExtensionPostInstallDialog>(
      web_contents, std::move(model));
  gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
  if (!native_window) {
    return;
  }

  auto* weak_delegate = delegate.get();

  ui::DialogModel::Builder dialog_model_builder(std::move(delegate));

  auto manage_shortcuts_callback =
      base::BindRepeating(&ExtensionPostInstallDialog::LinkClicked,
                          base::Unretained(weak_delegate));

  extensions::ConfigurePostInstallDialogModel(
      dialog_model_builder, weak_delegate->model(), manage_shortcuts_callback);

  // TODO(crbug.com/450296898): Add a sync or sign in promo in the footer if it
  // should be shown.

  std::unique_ptr<ui::DialogModel> dialog_model = dialog_model_builder.Build();
  ShowDialog(native_window, weak_delegate->model()->extension_id(),
             std::move(dialog_model));
}

}  // namespace extensions
