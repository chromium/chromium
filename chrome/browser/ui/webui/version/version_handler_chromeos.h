// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/version/version_handler.h"

// VersionHandlerChromeOS is responsible for loading the Chrome OS
// version.
class VersionHandlerChromeOS : public VersionHandler {
 public:
  VersionHandlerChromeOS();

  VersionHandlerChromeOS(const VersionHandlerChromeOS&) = delete;
  VersionHandlerChromeOS& operator=(const VersionHandlerChromeOS&) = delete;

  ~VersionHandlerChromeOS() override;

  // VersionHandler overrides:
  void OnJavascriptDisallowed() override;
  void HandleRequestVersionInfo(const base::Value::List& args) override;

  // Callbacks from chromeos::VersionLoader.
  void OnPlatformVersion(const std::optional<std::string>& version);
  void OnFirmwareVersion(const std::string& version);
  void OnArcAndArcAndroidSdkVersions(const std::string& version);

 private:
  base::WeakPtrFactory<VersionHandlerChromeOS> weak_factory_{this};
  static std::string GetArcAndArcAndroidSdkVersions();
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_
