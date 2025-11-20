// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_installed_waiter.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"
#include "ui/base/models/dialog_model.h"

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
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

  static void Show(Browser* browser,
                   std::unique_ptr<ExtensionPostInstallDialogModel> model);

  const ExtensionPostInstallDialogModel* model() const { return model_.get(); }

  void LinkClicked();

 private:
  base::WeakPtr<content::WebContents> web_contents_;

  const std::unique_ptr<ExtensionPostInstallDialogModel> model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POST_INSTALL_DIALOG_DELEGATE_H_
