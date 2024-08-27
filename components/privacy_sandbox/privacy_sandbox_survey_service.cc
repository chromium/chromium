// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_survey_service.h"

namespace privacy_sandbox {

PrivacySandboxSurveyService::PrivacySandboxSurveyService(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

PrivacySandboxSurveyService::~PrivacySandboxSurveyService() = default;

}  // namespace privacy_sandbox
