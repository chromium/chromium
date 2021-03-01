// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PLUGIN_VM_INTERNAL_PLUGIN_VM_INTERNAL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PLUGIN_VM_INTERNAL_PLUGIN_VM_INTERNAL_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// The WebUI for chrome://parallels-internal
class PluginVmInternalUI : public content::WebUIController {
 public:
  explicit PluginVmInternalUI(content::WebUI* web_ui);
  PluginVmInternalUI(const PluginVmInternalUI&) = delete;
  PluginVmInternalUI& operator=(const PluginVmInternalUI&) = delete;
  ~PluginVmInternalUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PLUGIN_VM_INTERNAL_PLUGIN_VM_INTERNAL_UI_H_
