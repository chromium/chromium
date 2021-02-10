// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/mojom/mojom_traits.h"

namespace mojo {

using AppStatus = chromeos::assistant::AppStatus;
using AndroidAppStatus = chromeos::libassistant::mojom::AndroidAppStatus;
using chromeos::assistant::AndroidAppInfo;
using chromeos::assistant::AssistantFeedback;
using chromeos::assistant::AssistantNotification;
using chromeos::libassistant::mojom::AndroidAppInfoDataView;
using chromeos::libassistant::mojom::AssistantFeedbackDataView;
using chromeos::libassistant::mojom::AssistantNotificationDataView;

////////////////////////////////////////////////////////////////////////////////
// AndroidAppStatus
////////////////////////////////////////////////////////////////////////////////

AndroidAppStatus EnumTraits<AndroidAppStatus, AppStatus>::ToMojom(
    AppStatus input) {
  switch (input) {
    case AppStatus::kUnknown:
      return AndroidAppStatus::kUnknown;
    case AppStatus::kAvailable:
      return AndroidAppStatus::kAvailable;
    case AppStatus::kUnavailable:
      return AndroidAppStatus::kUnavailable;
    case AppStatus::kVersionMismatch:
      return AndroidAppStatus::kVersionMismatch;
    case AppStatus::kDisabled:
      return AndroidAppStatus::kDisabled;
  }
}

bool EnumTraits<AndroidAppStatus, AppStatus>::FromMojom(AndroidAppStatus input,
                                                        AppStatus* output) {
  switch (input) {
    case AndroidAppStatus::kUnknown:
      *output = AppStatus::kUnknown;
      break;
    case AndroidAppStatus::kAvailable:
      *output = AppStatus::kAvailable;
      break;
    case AndroidAppStatus::kUnavailable:
      *output = AppStatus::kUnavailable;
      break;
    case AndroidAppStatus::kVersionMismatch:
      *output = AppStatus::kVersionMismatch;
      break;
    case AndroidAppStatus::kDisabled:
      *output = AppStatus::kDisabled;
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AndroidAppInfo
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::package_name(
    const AndroidAppInfo& input) {
  return input.package_name;
}

int64_t StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::version(
    const AndroidAppInfo& input) {
  return input.version;
}

const std::string&
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::localized_app_name(
    const AndroidAppInfo& input) {
  return input.localized_app_name;
}

const std::string& StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::intent(
    const AndroidAppInfo& input) {
  return input.intent;
}

chromeos::assistant::AppStatus
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::status(
    const AndroidAppInfo& input) {
  return input.status;
}

const std::string& StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::action(
    const AndroidAppInfo& input) {
  return input.action;
}

bool StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::Read(
    chromeos::libassistant::mojom::AndroidAppInfoDataView data,
    AndroidAppInfo* output) {
  if (!data.ReadPackageName(&output->package_name))
    return false;
  output->version = data.version();
  if (!data.ReadLocalizedAppName(&output->localized_app_name))
    return false;
  if (!data.ReadIntent(&output->intent))
    return false;
  if (!data.ReadStatus(&output->status))
    return false;
  if (!data.ReadAction(&output->action))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantNotification
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::server_id(
    const AssistantNotification& input) {
  return input.server_id;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::
    consistency_token(const AssistantNotification& input) {
  return input.consistency_token;
}

const std::string& StructTraits<
    AssistantNotificationDataView,
    AssistantNotification>::opaque_token(const AssistantNotification& input) {
  return input.opaque_token;
}

const std::string& StructTraits<
    AssistantNotificationDataView,
    AssistantNotification>::grouping_key(const AssistantNotification& input) {
  return input.grouping_key;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::
    obfuscated_gaia_id(const AssistantNotification& input) {
  return input.obfuscated_gaia_id;
}

bool StructTraits<AssistantNotificationDataView, AssistantNotification>::Read(
    chromeos::libassistant::mojom::AssistantNotificationDataView data,
    AssistantNotification* output) {
  if (!data.ReadServerId(&output->server_id))
    return false;
  if (!data.ReadConsistencyToken(&output->consistency_token))
    return false;
  if (!data.ReadOpaqueToken(&output->opaque_token))
    return false;
  if (!data.ReadGroupingKey(&output->grouping_key))
    return false;
  if (!data.ReadObfuscatedGaiaId(&output->obfuscated_gaia_id))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantFeedback
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AssistantFeedbackDataView, AssistantFeedback>::description(
    const AssistantFeedback& input) {
  return input.description;
}

bool StructTraits<AssistantFeedbackDataView, AssistantFeedback>::
    assistant_debug_info_allowed(const AssistantFeedback& input) {
  return input.assistant_debug_info_allowed;
}

base::span<const uint8_t>
StructTraits<AssistantFeedbackDataView, AssistantFeedback>::screenshot_png(
    const AssistantFeedback& input) {
  return input.screenshot_png;
}

bool StructTraits<AssistantFeedbackDataView, AssistantFeedback>::Read(
    chromeos::libassistant::mojom::AssistantFeedbackDataView data,
    AssistantFeedback* output) {
  if (!data.ReadDescription(&output->description))
    return false;
  output->assistant_debug_info_allowed = data.assistant_debug_info_allowed();
  if (!data.ReadScreenshotPng(&output->screenshot_png))
    return false;
  return true;
}

}  // namespace mojo
