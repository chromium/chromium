// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"

#include "base/command_line.h"
#include "chrome/common/crash_keys.h"
#include "content/public/common/content_switches.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"

ChromeExtensionsDispatcherDelegate::ChromeExtensionsDispatcherDelegate() {}

ChromeExtensionsDispatcherDelegate::~ChromeExtensionsDispatcherDelegate() {}

void ChromeExtensionsDispatcherDelegate::RequireWebViewModules(
    extensions::ScriptContext* context) {
  DCHECK(context->GetAvailability("webViewInternal").is_available());
  if (context->GetAvailability("chromeWebViewTag").is_available()) {
    context->module_system()->Require("chromeWebViewElement");
  }
}

void ChromeExtensionsDispatcherDelegate::OnActiveExtensionsUpdated(
    const std::set<std::string>& extension_ids) {
  // In single-process mode, the browser process reports the active extensions.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSingleProcess))
    return;
  crash_keys::SetActiveExtensions(extension_ids);
}
