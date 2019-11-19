// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chromeos/dbus/util/version_loader.h"

// VersionHandlerChromeOS is responsible for loading the Chrome OS
// version.
class VersionHandlerChromeOS : public VersionHandler {
 public:
  VersionHandlerChromeOS();
  ~VersionHandlerChromeOS() override;

  // VersionHandler overrides:
  void HandleRequestVersionInfo(const base::ListValue* args) override;

  // Callbacks from chromeos::VersionLoader.
  void OnVersion(const std::string& version);
  void OnOSFirmware(const std::string& version);
  void OnARCVersion(const std::string& version);

 private:
  base::WeakPtrFactory<VersionHandlerChromeOS> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VersionHandlerChromeOS);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_
