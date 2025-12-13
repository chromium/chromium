// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/extension_service.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
using ash::language_packs::GetPackStateCallback;
using ash::language_packs::LanguagePackManager;
using ash::language_packs::OnInstallCompleteCallback;
#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromeOsExtensionWrapper {
 public:
  ChromeOsExtensionWrapper();
  virtual ~ChromeOsExtensionWrapper();

#if BUILDFLAG(IS_CHROMEOS)
  virtual void ActivateSpeechEngine(Profile* profile);
  virtual void ReleaseSpeechEngine(Profile* profile);
  virtual void RequestLanguageInfo(const std::string& language,
                                   GetPackStateCallback callback);
  virtual void RequestLanguageInstall(const std::string& language,
                                      OnInstallCompleteCallback callback);

 private:
  void UpdateSpeechEngineKeepaliveCount(Profile* profile, bool increment);

  std::map<extensions::WorkerId, base::Uuid> keepalive_uuids_;

#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_CHROME_OS_EXTENSION_WRAPPER_H_
