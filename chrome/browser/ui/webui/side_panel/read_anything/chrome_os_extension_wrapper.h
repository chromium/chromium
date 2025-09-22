// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
using ash::language_packs::GetPackStateCallback;
using ash::language_packs::LanguagePackManager;
using ash::language_packs::OnInstallCompleteCallback;
#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromeOsExtensionWrapper {
 public:
  ChromeOsExtensionWrapper() = default;
  virtual ~ChromeOsExtensionWrapper() = default;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns true if the engine is already awake and does not invoke the
  // callback. Returns false if the engine is not yet awake and invokes the
  // callback when the wake attempt returns, successfully or unsuccessfuly.
  virtual bool WakeEngine(Profile* profile,
                          base::OnceCallback<void(bool)> callback);

  virtual void RequestLanguageInfo(const std::string& language,
                                   GetPackStateCallback callback);
  virtual void RequestLanguageInstall(const std::string& language,
                                      OnInstallCompleteCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_
