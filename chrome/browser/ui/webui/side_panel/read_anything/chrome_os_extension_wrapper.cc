// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/chrome_os_extension_wrapper.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/session/session_controller.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"

ChromeOsExtensionWrapper::ChromeOsExtensionWrapper() = default;
ChromeOsExtensionWrapper::~ChromeOsExtensionWrapper() = default;

void ChromeOsExtensionWrapper::ActivateSpeechEngine(Profile* profile) {
  UpdateSpeechEngineKeepaliveCount(profile, /*increment=*/true);
}

void ChromeOsExtensionWrapper::ReleaseSpeechEngine(Profile* profile) {
  UpdateSpeechEngineKeepaliveCount(profile, /*increment=*/false);
}

void ChromeOsExtensionWrapper::UpdateSpeechEngineKeepaliveCount(
    Profile* profile,
    bool increment) {
  auto* process_manager = extensions::ProcessManager::Get(profile);
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(
          extension_misc::kGoogleSpeechSynthesisExtensionId);
  if (!extension) {
    return;
  }

  auto activity_type = extensions::Activity::Type::ACCESSIBILITY;
  const std::string& extra_data = std::string();
  // The manifest V3 engine is service worker based, while the V2 engine is not.
  // Ensure we handle both cases until the V2 engine is deleted entirely.
  if (extensions::BackgroundInfo::IsServiceWorkerBased(extension)) {
    std::vector<extensions::WorkerId> all_worker_ids =
        process_manager->GetServiceWorkersForExtension(
            extension_misc::kGoogleSpeechSynthesisExtensionId);
    if (increment) {
      for (const extensions::WorkerId& worker_id : all_worker_ids) {
        base::Uuid uuid = process_manager->IncrementServiceWorkerKeepaliveCount(
            worker_id,
            content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
            extensions::Activity::Type::ACCESSIBILITY, std::string());
        keepalive_uuids_[worker_id] = uuid;
      }
    } else {
      for (const auto& [worker_id, uuid] : keepalive_uuids_) {
        process_manager->DecrementServiceWorkerKeepaliveCount(
            worker_id, uuid, activity_type, extra_data);
      }
    }
  } else {
    if (increment) {
      process_manager->IncrementLazyKeepaliveCount(extension, activity_type,
                                                   extra_data);
    } else {
      process_manager->DecrementLazyKeepaliveCount(extension, activity_type,
                                                   extra_data);
    }
  }
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
