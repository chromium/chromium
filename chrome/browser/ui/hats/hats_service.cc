// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

HatsService::SurveyMetadata::SurveyMetadata() = default;

HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::HatsService(Profile* profile) : profile_(profile) {
  hats::GetActiveSurveyConfigs(survey_configs_by_triggers_);
}

HatsService::~HatsService() = default;
