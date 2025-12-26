// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/dialog_model.h"

// Model delegate for the dialog that provides feedback to the user upon
// successful installation of an extension.
class ExtensionPostInstallDialogDelegate : public ui::DialogModelDelegate {
 public:
  ExtensionPostInstallDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<ExtensionPostInstallDialogModel> model);
  ExtensionPostInstallDialogDelegate(
      const ExtensionPostInstallDialogDelegate&) = delete;
  ExtensionPostInstallDialogDelegate& operator=(
      const ExtensionPostInstallDialogDelegate&) = delete;
  ~ExtensionPostInstallDialogDelegate() override;

  const ExtensionPostInstallDialogModel* model() const { return model_.get(); }

  void LinkClicked();

 private:
  base::WeakPtr<content::WebContents> web_contents_;

  const std::unique_ptr<ExtensionPostInstallDialogModel> model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_
