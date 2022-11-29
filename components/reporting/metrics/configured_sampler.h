// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_CONFIGURED_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_CONFIGURED_SAMPLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

class ReportingSettings;

// Class to access a Sampler intsance along with its enabling setting path and
// default value.
class ConfiguredSampler {
 public:
  ConfiguredSampler(std::unique_ptr<Sampler> sampler,
                    base::StringPiece enable_setting_path,
                    bool setting_enabled_default_value,
                    ReportingSettings* reporting_settings);

  ConfiguredSampler(const ConfiguredSampler& other) = delete;
  ConfiguredSampler& operator=(const ConfiguredSampler& other) = delete;

  ~ConfiguredSampler();

  // Return raw pointer to the sampler, ConfiguredSampler should outlive the
  // consumer.
  Sampler* GetSampler() const;

  // Get reporting setting path for the sampler, ConfiguredSampler should
  // outlive the consumer.
  const std::string& GetEnableSettingPath() const;

  // Get reporting setting default value if the setting is not set.
  bool GetSettingEnabledDefaultValue() const;

  bool IsReportingEnabled() const;

 private:
  const std::unique_ptr<Sampler> sampler_;
  const std::string enable_setting_path_;
  bool setting_enabled_default_value_;
  raw_ptr<ReportingSettings> reporting_settings_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_CONFIGURED_SAMPLER_H_
