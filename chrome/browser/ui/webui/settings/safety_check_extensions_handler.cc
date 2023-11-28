// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_extensions_handler.h"

#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace settings {

namespace {

// `kPrefAcknowledgeSafetyCheckWarning` should mirror the definition in
// chrome/browser/extensions/api/developer_private/developer_private_api.h.
constexpr extensions::PrefMap kPrefAcknowledgeSafetyCheckWarning = {
    "ack_safety_check_warning", extensions::PrefType::kBool,
    extensions::PrefScope::kExtensionSpecific};

}  // namespace

SafetyCheckExtensionsHandler::SafetyCheckExtensionsHandler(Profile* profile)
    : profile_(profile) {
  prefs_observation_.Observe(ExtensionPrefs::Get(profile_));
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}

SafetyCheckExtensionsHandler::~SafetyCheckExtensionsHandler() = default;

void SafetyCheckExtensionsHandler::SetCWSInfoServiceForTest(
    extensions::CWSInfoService* cws_info_service) {
  cws_info_service_ = cws_info_service;
}

void SafetyCheckExtensionsHandler::SetTriggeringExtensionsForTest(
    extensions::ExtensionId extension_id) {
  triggering_extensions_.insert(std::move(extension_id));
}

bool SafetyCheckExtensionsHandler::CheckExtensionForTrigger(
    const extensions::Extension& extension) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefsFactory::GetForBrowserContext(profile_);
  bool warning_acked = false;
  extension_prefs->ReadPrefAsBoolean(
      extension.id(), kPrefAcknowledgeSafetyCheckWarning, &warning_acked);
  bool is_extension = extension.is_extension() || extension.is_shared_module();
  // If the user has previously acknowledged the warning on this
  // extension and chosen to keep it, we will not show an additional
  // safety hub warning. We also will not show warnings on Chrome apps.
  if (warning_acked || !is_extension) {
    return false;
  }
  absl::optional<extensions::CWSInfoService::CWSInfo> extension_info =
      cws_info_service_->GetCWSInfo(extension);
  if (extension_info.has_value() && extension_info->is_present) {
    switch (extension_info->violation_type) {
      case extensions::CWSInfoService::CWSViolationType::kMalware:
      case extensions::CWSInfoService::CWSViolationType::kPolicy:
        return true;
      case extensions::CWSInfoService::CWSViolationType::kNone:
      case extensions::CWSInfoService::CWSViolationType::kMinorPolicy:
      case extensions::CWSInfoService::CWSViolationType::kUnknown:
        if (extension_info->unpublished_long_ago) {
          return true;
        }
        break;
    }
  }
  return false;
}

void SafetyCheckExtensionsHandler::HandleGetNumberOfExtensionsThatNeedReview(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id,
                            base::Value(GetNumberOfExtensionsThatNeedReview()));
}

int SafetyCheckExtensionsHandler::GetNumberOfExtensionsThatNeedReview() {
  triggering_extensions_.clear();
  if (!base::FeatureList::IsEnabled(extensions::kCWSInfoService)) {
    return 0;
  }

  if (cws_info_service_ == nullptr) {
    cws_info_service_ = extensions::CWSInfoService::Get(profile_);
  }

  const extensions::ExtensionSet all_installed_extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  for (const auto& extension : all_installed_extensions) {
    // Check if the extension is installed by a policy.
    if (!extensions::Manifest::IsPolicyLocation(extension->location()) &&
        CheckExtensionForTrigger(*extension.get())) {
      triggering_extensions_.insert(std::move(extension->id()));
    }
  }
  return triggering_extensions_.size();
}

void SafetyCheckExtensionsHandler::UpdateNumberOfExtensionsThatNeedReview() {
  AllowJavascript();
  int num_extensions = triggering_extensions_.size();
  FireWebUIListener("extensions-review-list-maybe-changed", num_extensions);
}

void SafetyCheckExtensionsHandler::OnExtensionPrefsUpdated(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  auto extension_ptr = triggering_extensions_.find(extension_id);
  if (extension_ptr != triggering_extensions_.end() &&
      (!extension || !CheckExtensionForTrigger(*extension))) {
    triggering_extensions_.erase(extension_ptr);
    UpdateNumberOfExtensionsThatNeedReview();
  }
}

void SafetyCheckExtensionsHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  auto extension_ptr = triggering_extensions_.find(extension->id());
  if (extension_ptr != triggering_extensions_.end()) {
    triggering_extensions_.erase(extension_ptr);
    UpdateNumberOfExtensionsThatNeedReview();
  }
}

void SafetyCheckExtensionsHandler::OnJavascriptAllowed() {}

void SafetyCheckExtensionsHandler::OnJavascriptDisallowed() {}

void SafetyCheckExtensionsHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getNumberOfExtensionsThatNeedReview",
      base::BindRepeating(&SafetyCheckExtensionsHandler::
                              HandleGetNumberOfExtensionsThatNeedReview,
                          base::Unretained(this)));
}

}  // namespace settings
