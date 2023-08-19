// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSION_CONTROL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSION_CONTROL_HANDLER_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

// A class that provides a message handler that disables extensions, intended
// for use in, for example, settings pages or password manager.
class ExtensionControlHandler : public content::WebUIMessageHandler {
 public:
  ExtensionControlHandler();

  ExtensionControlHandler(const ExtensionControlHandler&) = delete;
  ExtensionControlHandler& operator=(const ExtensionControlHandler&) = delete;

  ~ExtensionControlHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Handler for the "disableExtension" message. Extension ID is passed as the
  // single string argument.
  void HandleDisableExtension(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Handler for the "openExtensionPageInLacros" message. Extension ID is passed
  // as the single string argument.
  void HandleOpenExtensionPageInLacros(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSION_CONTROL_HANDLER_H_
