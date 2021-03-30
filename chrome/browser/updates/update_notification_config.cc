// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"  // nogncheck

namespace updates {
namespace {
// Helper routines to get Finch experiment parameter. If no Finch seed was
// found, use the |default_value|. The |name| should match an experiment
// parameter in Finch server configuration.
int GetFinchConfigInt(const std::string& name, int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kInlineUpdateFlow, name, default_value);
}

bool GetFinchConfigBool(const std::string& name, bool default_value) {
  return base::GetFieldTrialParamByFeatureAsBool(
      chrome::android::kInlineUpdateFlow, name, default_value);
}

}  // namespace

// Default update notification schedule initial interval in days.
constexpr int kDefaultUpdateNotificationInitInterval = 21;

// Default update notification schedule maximum interval in days.
constexpr int kDefaultUpdateNotificationMaxInterval = 90;

// Default start clock of deliver window in the morning.
constexpr int kDefaultDeliverWindowMorningStart = 5;

// Default end clock of deliver window in the morning.
constexpr int kDefaultDeliverWindowMorningEnd = 7;

// Default start clock of deliver window in the evening.
constexpr int kDefaultDeliverWindowEveningStart = 18;

// Default end clock of deliver window in the evening.
constexpr int kDefaultDeliverWindowEveningEnd = 20;

// Default update notification state.
constexpr bool kDefaultUpdateNotificationState = false;

std::unique_ptr<UpdateNotificationConfig> UpdateNotificationConfig::Create() {
  return std::make_unique<UpdateNotificationConfig>();
}

std::unique_ptr<UpdateNotificationConfig>
UpdateNotificationConfig::CreateFromFinch() {
  std::unique_ptr<UpdateNotificationConfig> config =
      std::make_unique<UpdateNotificationConfig>();

  config->is_enabled = GetFinchConfigBool(kUpdateNotificationStateParamName,
                                          kDefaultUpdateNotificationState);

  config->init_interval = base::TimeDelta::FromDays(
      GetFinchConfigInt(kUpdateNotificationInitIntervalParamName,
                        kDefaultUpdateNotificationInitInterval));

  config->max_interval = base::TimeDelta::FromDays(
      GetFinchConfigInt(kUpdateNotificationMaxIntervalParamName,
                        kDefaultUpdateNotificationMaxInterval));

  int morning_window_start =
      GetFinchConfigInt(kUpdateNotificationDeliverWindowMorningStartParamName,
                        kDefaultDeliverWindowMorningStart);
  int morning_window_end =
      GetFinchConfigInt(kUpdateNotificationDeliverWindowMorningEndParamName,
                        kDefaultDeliverWindowMorningEnd);
  config->deliver_window_morning = {
      base::TimeDelta::FromHours(morning_window_start),
      base::TimeDelta::FromHours(morning_window_end)};

  int evening_window_start =
      GetFinchConfigInt(kUpdateNotificationDeliverWindowEveningStartParamName,
                        kDefaultDeliverWindowEveningStart);
  int evening_window_end =
      GetFinchConfigInt(kUpdateNotificationDeliverWindowEveningEndParamName,
                        kDefaultDeliverWindowEveningEnd);
  config->deliver_window_evening = {
      base::TimeDelta::FromHours(evening_window_start),
      base::TimeDelta::FromHours(evening_window_end)};

  return config;
}

UpdateNotificationConfig::UpdateNotificationConfig()
    : is_enabled(true),
      init_interval(
          base::TimeDelta::FromDays(kDefaultUpdateNotificationInitInterval)),
      max_interval(
          base::TimeDelta::FromDays(kDefaultUpdateNotificationMaxInterval)),
      deliver_window_morning(
          base::TimeDelta::FromHours(kDefaultDeliverWindowMorningStart),
          base::TimeDelta::FromHours(kDefaultDeliverWindowMorningEnd)),
      deliver_window_evening(
          base::TimeDelta::FromHours(kDefaultDeliverWindowEveningStart),
          base::TimeDelta::FromHours(kDefaultDeliverWindowEveningEnd)) {}

UpdateNotificationConfig::~UpdateNotificationConfig() = default;

}  // namespace updates
