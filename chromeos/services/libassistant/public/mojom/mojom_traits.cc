// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/mojom/mojom_traits.h"

namespace mojo {

using AppStatus = chromeos::assistant::AppStatus;
using AndroidAppStatus = chromeos::libassistant::mojom::AndroidAppStatus;
using chromeos::assistant::AndroidAppInfo;
using chromeos::libassistant::mojom::AndroidAppInfoDataView;

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

std::string StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::package_name(
    const AndroidAppInfo& input) {
  return input.package_name;
}

int64_t StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::version(
    const AndroidAppInfo& input) {
  return input.version;
}

std::string
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::localized_app_name(
    const AndroidAppInfo& input) {
  return input.localized_app_name;
}

std::string StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::intent(
    const AndroidAppInfo& input) {
  return input.intent;
}

chromeos::assistant::AppStatus
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::status(
    const AndroidAppInfo& input) {
  return input.status;
}

std::string StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::action(
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

}  // namespace mojo
