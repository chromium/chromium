// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Textfield;
}

class CryptoModulePasswordDialogView : public views::DialogDelegateView,
                                       public views::TextfieldController {
 public:
  METADATA_HEADER(CryptoModulePasswordDialogView);
  CryptoModulePasswordDialogView(const std::string& slot_name,
                                 CryptoModulePasswordReason reason,
                                 const std::string& server,
                                 CryptoModulePasswordCallback callback);
  CryptoModulePasswordDialogView(const CryptoModulePasswordDialogView&) =
      delete;
  CryptoModulePasswordDialogView& operator=(
      const CryptoModulePasswordDialogView&) = delete;
  ~CryptoModulePasswordDialogView() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CryptoModulePasswordDialogViewTest,
                           AcceptUsesPassword);
  FRIEND_TEST_ALL_PREFIXES(CryptoModulePasswordDialogViewTest,
                           CancelDoesntUsePassword);

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetWindowTitle() const override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& keystroke) override;

  // Initialize views and layout.
  void Init(const std::string& server,
            const std::string& slot_name,
            CryptoModulePasswordReason reason);

  views::Label* reason_label_;
  views::Label* password_label_;
  views::Textfield* password_entry_;

  CryptoModulePasswordCallback callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
