// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_ANDROID_APP_INFO_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_ANDROID_APP_INFO_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {
namespace assistant {

// Models status of an app.
enum class AppStatus {
  kUnknown,
  kAvailable,
  kUnavailable,
  kVersionMismatch,
  kDisabled,
};

// Models an Android app.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS) AndroidAppInfo {
  AndroidAppInfo();
  AndroidAppInfo(const AndroidAppInfo& suggestion);
  AndroidAppInfo& operator=(const AndroidAppInfo&);
  AndroidAppInfo(AndroidAppInfo&& suggestion);
  AndroidAppInfo& operator=(AndroidAppInfo&&);
  ~AndroidAppInfo();

  // Unique name to identify a specific app.
  std::string package_name;

  // Version number of the app.
  int version{0};

  // Localized app name.
  std::string localized_app_name;

  // Intent data to operate on.
  std::string intent;

  // Status of the app.
  AppStatus status{AppStatus::kUnknown};

  // The general action to be performed, such as ACTION_VIEW, ACTION_MAIN, etc.
  std::string action;
};

}  // namespace assistant
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::assistant {
using ::chromeos::assistant::AndroidAppInfo;
using ::chromeos::assistant::AppStatus;
}  // namespace ash::assistant

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_ANDROID_APP_INFO_H_
