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

namespace settings {

namespace {

// `kPrefAcknowledgeSafetyCheckWarning` should mirror the definition in
// chrome/browser/extensions/api/developer_private/developer_private_api.h.
constexpr extensions::PrefMap kPrefAcknowledgeSafetyCheckWarning = {
    "ack_safety_check_warning", extensions::PrefType::kBool,
    extensions::PrefScope::kExtensionSpecific};

}  // namespace

SafetyCheckExtensionsHandler::SafetyCheckExtensionsHandler(Profile* profile)
    : profile_(profile) {}

SafetyCheckExtensionsHandler::~SafetyCheckExtensionsHandler() = default;

void SafetyCheckExtensionsHandler::HandleGetNumberOfExtensionsThatNeedReview(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id,
                            base::Value(GetNumberOfExtensionsThatNeedReview()));
}

void SafetyCheckExtensionsHandler::
    HandleUpdateNumberOfExtensionsThatNeedReview() {
  AllowJavascript();
  FireWebUIListener("extensions-review-list-maybe-changed",
                    GetNumberOfExtensionsThatNeedReview());
}

void SafetyCheckExtensionsHandler::SetCWSInfoServiceForTest(
    extensions::CWSInfoService* cws_info_service) {
  cws_info_service_ = cws_info_service;
}

int SafetyCheckExtensionsHandler::GetNumberOfExtensionsThatNeedReview() {
  int num_extensions_that_need_review = 0;
  if (!base::FeatureList::IsEnabled(extensions::kCWSInfoService)) {
    return num_extensions_that_need_review;
  }

  if (cws_info_service_ == nullptr) {
    cws_info_service_ = extensions::CWSInfoService::Get(profile_);
  }

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefsFactory::GetForBrowserContext(profile_);

  for (const auto& extension_id : extension_prefs->GetExtensions()) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::EVERYTHING);
    // Check if the extension is installed by a policy.
    if (!extension ||
        extensions::Manifest::IsPolicyLocation(extension->location())) {
      continue;
    }
    bool warning_acked = false;
    extension_prefs->ReadPrefAsBoolean(
        extension->id(), kPrefAcknowledgeSafetyCheckWarning, &warning_acked);
    // If the user has previously acknowledged the warning on this
    // extension and chosen to keep it, we will not show an additional
    // safety hub warning.
    if (warning_acked) {
      continue;
    }
    absl::optional<extensions::CWSInfoService::CWSInfo> extension_info =
        cws_info_service_->GetCWSInfo(*extension);
    if (extension_info.has_value() && extension_info->is_present) {
      switch (extension_info->violation_type) {
        case extensions::CWSInfoService::CWSViolationType::kMalware:
        case extensions::CWSInfoService::CWSViolationType::kPolicy:
          num_extensions_that_need_review++;
          break;
        case extensions::CWSInfoService::CWSViolationType::kNone:
        case extensions::CWSInfoService::CWSViolationType::kMinorPolicy:
        case extensions::CWSInfoService::CWSViolationType::kUnknown:
          if (extension_info->unpublished_long_ago) {
            num_extensions_that_need_review++;
          }
          break;
      }
    }
  }
  return num_extensions_that_need_review;
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
