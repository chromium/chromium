// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/chrome_os_extension_wrapper.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/session/session_controller.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/process_manager.h"

bool ChromeOsExtensionWrapper::WakeEngine(
    Profile* profile,
    base::OnceCallback<void(bool)> callback) {
  return !extensions::ProcessManager::Get(profile)->WakeEventPage(
      extension_misc::kGoogleSpeechSynthesisExtensionId, std::move(callback));
}

void ChromeOsExtensionWrapper::RequestLanguageInfo(
    const std::string& language,
    GetPackStateCallback callback) {
  LanguagePackManager::GetPackState(ash::language_packs::kTtsFeatureId,
                                    language, std::move(callback));
}

void ChromeOsExtensionWrapper::RequestLanguageInstall(
    const std::string& language,
    OnInstallCompleteCallback callback) {
  LanguagePackManager::InstallPack(ash::language_packs::kTtsFeatureId, language,
                                   std::move(callback));
}
#endif
