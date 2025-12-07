// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fjord_oobe_util.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

namespace ash::fjord_util {

namespace {
const std::set<std::string>& kFjordOobeAllowedLanguages = {"en"};
}

bool ShouldShowFjordOobe() {
  return features::IsFjordOobeForceEnabled() ||
         (policy::EnrollmentRequisitionManager::IsCuttlefishDevice() &&
          features::IsFjordOobeEnabled());
}

bool IsAllowlistedLanguage(std::string_view language_code) {
  return kFjordOobeAllowedLanguages.contains(language_code.data());
}

const std::set<std::string>& GetAllowlistedLanguagesForTesting() {
  return kFjordOobeAllowedLanguages;
}

}  // namespace ash::fjord_util
