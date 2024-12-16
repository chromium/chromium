// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

class CredentialLeakDialogController;

class CredentialLeakDialogView : public views::DialogDelegateView,
                                 public CredentialLeakPrompt {
  METADATA_HEADER(CredentialLeakDialogView, views::DialogDelegateView)

 public:
  CredentialLeakDialogView(CredentialLeakDialogController* controller,
                           content::WebContents* web_contents);
  CredentialLeakDialogView(const CredentialLeakDialogView&) = delete;
  CredentialLeakDialogView& operator=(const CredentialLeakDialogView&) = delete;
  ~CredentialLeakDialogView() override;

  // CredentialsLeakedPrompt:
  void ShowCredentialLeakPrompt() override;
  void ControllerGone() override;

 private:
  // views::DialogDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  raw_ptr<CredentialLeakDialogController> controller_ = nullptr;
  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_
