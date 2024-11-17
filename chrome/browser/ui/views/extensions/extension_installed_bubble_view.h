// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"
#include "chrome/browser/ui/extensions/extension_installed_waiter.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "extensions/common/extension.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
class ExtensionInstalledBubbleView : public BubbleSignInPromoDelegate,
                                     public views::BubbleDialogDelegateView {
  METADATA_HEADER(ExtensionInstalledBubbleView, views::BubbleDialogDelegateView)

 public:
  ExtensionInstalledBubbleView(
      Browser* browser,
      std::unique_ptr<ExtensionInstalledBubbleModel> model);
  ExtensionInstalledBubbleView(const ExtensionInstalledBubbleView&) = delete;
  ExtensionInstalledBubbleView& operator=(const ExtensionInstalledBubbleView&) =
      delete;
  ~ExtensionInstalledBubbleView() override;

  static void Show(Browser* browser,
                   std::unique_ptr<ExtensionInstalledBubbleModel> model);

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  const ExtensionInstalledBubbleModel* model() const { return model_.get(); }

  // Simulate a sign in from this bubble with `account_info`.
  void SignInForTesting(const AccountInfo& account_info);

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // BubbleSignInPromoDelegate:
  void OnSignIn(const AccountInfo& account_info) override;

  void LinkClicked();

  const raw_ptr<Browser> browser_;
  const std::unique_ptr<ExtensionInstalledBubbleModel> model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
