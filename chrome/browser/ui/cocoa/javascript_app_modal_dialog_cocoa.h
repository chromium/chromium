// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/remote_cocoa/app_shim/alert.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class PopunderPreventer;

namespace javascript_dialogs {
class AppModalDialogController;
class AppModalDialogView;
}

namespace remote_cocoa {
class AlertBridge;
}

class JavaScriptAppModalDialogCocoa
    : public javascript_dialogs::AppModalDialogView {
 public:
  static javascript_dialogs::AppModalDialogView* CreateNativeJavaScriptDialog(
      javascript_dialogs::AppModalDialogController* controller);

  // Overridden from NativeAppModalDialog:
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

 private:
  explicit JavaScriptAppModalDialogCocoa(
      javascript_dialogs::AppModalDialogController* controller);
  JavaScriptAppModalDialogCocoa(const JavaScriptAppModalDialogCocoa&) = delete;
  JavaScriptAppModalDialogCocoa& operator=(
      const JavaScriptAppModalDialogCocoa&) = delete;
  ~JavaScriptAppModalDialogCocoa() override;

  // Return the parameters to use for the alert.
  remote_cocoa::mojom::AlertBridgeInitParamsPtr GetAlertParams();

  // Called when the alert completes. Deletes |this|.
  void OnAlertFinished(remote_cocoa::mojom::AlertDisposition disposition,
                       const std::u16string& prompt_text,
                       bool suppress_js_messages);

  // Called if there is an error connecting to the alert process. Deletes
  // |this|.
  void OnMojoDisconnect();

  // Mojo interface to the NSAlert.
  mojo::Remote<remote_cocoa::mojom::AlertBridge> alert_bridge_remote_;
  raw_ptr<remote_cocoa::mojom::AlertBridge, DanglingUntriaged> alert_bridge_;

  std::unique_ptr<javascript_dialogs::AppModalDialogController> controller_;
  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  int num_buttons_ = 0;
  bool is_showing_ = false;

  base::WeakPtrFactory<JavaScriptAppModalDialogCocoa> weak_factory_;
};

#endif  // CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
