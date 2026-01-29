// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

namespace default_browser {

// The "Modal" entrypoint for the Windows Default Browser. This dialog
// displays a WebUI page to provide prompts that help users navigate OS
// settings to set Chrome as the default browser based the DefaultBrowserSetter
// behavior.
class DefaultBrowserModalDialog : public ui::WebDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDefaultBrowserModalDialogId);

  // Shows the modal dialog. This is triggered by the DefaultBrowserManager when
  // the STARTUP_MODAL entrypoint is selected.
  // The modal have two variants, one with the Windows Settings illustration and
  // the other without it. When `use_settings_illustration` is true, the modal
  // will display the Windows Settings illustration.
  static void Show(Profile* profile,
                   gfx::NativeView parent,
                   bool use_settings_illustration);

  explicit DefaultBrowserModalDialog(bool use_settings_illustration);
  ~DefaultBrowserModalDialog() override;

  DefaultBrowserModalDialog(const DefaultBrowserModalDialog&) = delete;
  DefaultBrowserModalDialog& operator=(const DefaultBrowserModalDialog&) =
      delete;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
