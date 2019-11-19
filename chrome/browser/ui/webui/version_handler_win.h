// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_WIN_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/version_handler.h"

// VersionHandlerWindows is responsible for loading the Windows OS version.
class VersionHandlerWindows : public VersionHandler {
 public:
  VersionHandlerWindows();
  ~VersionHandlerWindows() override;

  // VersionHandler overrides:
  void HandleRequestVersionInfo(const base::ListValue* args) override;

  // Callbacks from windows::VersionLoader.
  void OnVersion(const std::string& version);

  // Expose the |FullWindowsVersion| defined in the .cc file for testing.
  static std::string GetFullWindowsVersionForTesting();

 private:
  base::WeakPtrFactory<VersionHandlerWindows> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VersionHandlerWindows);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_WIN_H_
