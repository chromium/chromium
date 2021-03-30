// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/javascript_app_modal_dialog_cocoa.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/remote_cocoa/app_shim/alert.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"

using remote_cocoa::mojom::AlertDisposition;

////////////////////////////////////////////////////////////////////////////////
// JavaScriptAppModalDialogCocoa:

JavaScriptAppModalDialogCocoa::JavaScriptAppModalDialogCocoa(
    javascript_dialogs::AppModalDialogController* controller)
    : controller_(controller),
      popunder_preventer_(new PopunderPreventer(controller->web_contents())),
      weak_factory_(this) {}

JavaScriptAppModalDialogCocoa::~JavaScriptAppModalDialogCocoa() {}

remote_cocoa::mojom::AlertBridgeInitParamsPtr
JavaScriptAppModalDialogCocoa::GetAlertParams() {
  remote_cocoa::mojom::AlertBridgeInitParamsPtr params =
      remote_cocoa::mojom::AlertBridgeInitParams::New();
  params->title = controller_->title();
  params->message_text = controller_->message_text();

  // Set a blank icon for dialogs with text provided by the page.
  // "onbeforeunload" dialogs don't have text provided by the page, so it's
  // OK to use the app icon.
  params->hide_application_icon = !controller_->is_before_unload_dialog();

  // Determine the names of the dialog buttons based on the flags. "Default"
  // is the OK button. "Other" is the cancel button. We don't use the
  // "Alternate" button in NSRunAlertPanel.
  params->primary_button_text = l10n_util::GetStringUTF16(IDS_APP_OK);
  switch (controller_->javascript_dialog_type()) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      num_buttons_ = 1;
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      num_buttons_ = 2;
      if (controller_->is_before_unload_dialog()) {
        if (controller_->is_reload()) {
          params->primary_button_text = l10n_util::GetStringUTF16(
              IDS_BEFORERELOAD_MESSAGEBOX_OK_BUTTON_LABEL);
        } else {
          params->primary_button_text = l10n_util::GetStringUTF16(
              IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
        }
      }
      params->secondary_button_text.emplace(
          l10n_util::GetStringUTF16(IDS_APP_CANCEL));
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      num_buttons_ = 2;
      params->secondary_button_text.emplace(
          l10n_util::GetStringUTF16(IDS_APP_CANCEL));
      params->text_field_text.emplace(controller_->default_prompt_text());
      break;

    default:
      NOTREACHED();
  }
  return params;
}

void JavaScriptAppModalDialogCocoa::OnAlertFinished(
    AlertDisposition disposition,
    const std::u16string& text_field_value,
    bool check_box_value) {
  switch (disposition) {
    case AlertDisposition::PRIMARY_BUTTON:
      controller_->OnAccept(text_field_value, check_box_value);
      break;
    case AlertDisposition::SECONDARY_BUTTON:
      // If the user wants to stay on this page, stop quitting (if a quit is in
      // progress).
      if (controller_->is_before_unload_dialog())
        chrome_browser_application_mac::CancelTerminate();
      controller_->OnCancel(check_box_value);
      break;
    case AlertDisposition::CLOSE:
      controller_->OnClose();
      break;
  }
  if (Browser* browser = BrowserList::GetInstance()->GetLastActive())
    browser->window()->Show();
  delete this;
}

void JavaScriptAppModalDialogCocoa::OnMojoDisconnect() {
  controller_->OnClose();
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// JavaScriptAppModalDialogCocoa, NativeAppModalDialog implementation:

void JavaScriptAppModalDialogCocoa::ShowAppModalDialog() {
  is_showing_ = true;

  mojo::PendingReceiver<remote_cocoa::mojom::AlertBridge> bridge_receiver =
      alert_bridge_.BindNewPipeAndPassReceiver();
  alert_bridge_.set_disconnect_handler(
      base::BindOnce(&JavaScriptAppModalDialogCocoa::OnMojoDisconnect,
                     weak_factory_.GetWeakPtr()));
  // If the alert is from a window that is out of process then use the
  // remote_cocoa::ApplicationHost for that window to create the alert.
  // Otherwise create an AlertBridge in-process (but still communicate with it
  // over mojo).
  auto* application_host = remote_cocoa::ApplicationHost::GetForNativeView(
      controller_->web_contents()->GetNativeView());
  if (application_host)
    application_host->GetApplication()->CreateAlert(std::move(bridge_receiver));
  else
    ignore_result(new remote_cocoa::AlertBridge(std::move(bridge_receiver)));
  alert_bridge_->Show(
      GetAlertParams(),
      base::BindOnce(&JavaScriptAppModalDialogCocoa::OnAlertFinished,
                     weak_factory_.GetWeakPtr()));
}

void JavaScriptAppModalDialogCocoa::ActivateAppModalDialog() {
}

void JavaScriptAppModalDialogCocoa::CloseAppModalDialog() {
  // This function expects that controller_->OnClose will be called before this
  // function completes.
  OnAlertFinished(AlertDisposition::CLOSE, std::u16string(),
                  false /* check_box_value */);
}

void JavaScriptAppModalDialogCocoa::AcceptAppModalDialog() {
  // Note that for out-of-process dialogs, we cannot find out the actual
  // prompt text or suppression checkbox state in time (because the caller
  // expects that OnAlertFinished be called before the function ends), so just
  // use the initial values.
  OnAlertFinished(AlertDisposition::PRIMARY_BUTTON,
                  controller_->default_prompt_text(),
                  false /* check_box_value */);
}

void JavaScriptAppModalDialogCocoa::CancelAppModalDialog() {
  OnAlertFinished(AlertDisposition::SECONDARY_BUTTON, std::u16string(), false
                  /* check_box_value */);
}

bool JavaScriptAppModalDialogCocoa::IsShowing() const {
  return is_showing_;
}

void InstallChromeJavaScriptAppModalDialogViewFactory() {
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->SetNativeDialogFactory(base::BindRepeating(
          [](javascript_dialogs::AppModalDialogController* controller) {
            javascript_dialogs::AppModalDialogView* view =
                new JavaScriptAppModalDialogCocoa(controller);
            // Match Views by activating the tab during creation (rather than
            // when showing).
            controller->web_contents()->GetDelegate()->ActivateContents(
                controller->web_contents());
            return view;
          }));
}
