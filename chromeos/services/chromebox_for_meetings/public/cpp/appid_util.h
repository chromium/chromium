// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_
#define CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {
namespace cfm {

// Returns true if the id provided matches a valid CfM PA/PWA appid.
COMPONENT_EXPORT(CHROMEOS_CFMSERVICE)
bool IsChromeboxForMeetingsHashedAppId(const std::string& app_id);

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_
