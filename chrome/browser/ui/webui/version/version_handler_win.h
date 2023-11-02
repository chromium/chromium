// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_WIN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/version/version_handler.h"

// VersionHandlerWindows is responsible for loading the Windows OS version.
class VersionHandlerWindows : public VersionHandler {
 public:
  VersionHandlerWindows();

  VersionHandlerWindows(const VersionHandlerWindows&) = delete;
  VersionHandlerWindows& operator=(const VersionHandlerWindows&) = delete;

  ~VersionHandlerWindows() override;

  // VersionHandler overrides:
  void HandleRequestVersionInfo(const base::Value::List& args) override;

  // Callbacks from windows::VersionLoader.
  void OnVersion(const std::string& version);

  // Expose the |FullWindowsVersion| defined in the .cc file for testing.
  static std::string GetFullWindowsVersionForTesting();

 private:
  base::WeakPtrFactory<VersionHandlerWindows> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_WIN_H_
