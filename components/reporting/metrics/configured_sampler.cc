// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/configured_sampler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

ConfiguredSampler::ConfiguredSampler(std::unique_ptr<Sampler> sampler,
                                     base::StringPiece enable_setting_path,
                                     bool setting_enabled_default_value)
    : sampler_(std::move(sampler)),
      enable_setting_path_(enable_setting_path),
      setting_enabled_default_value_(setting_enabled_default_value) {}

ConfiguredSampler::~ConfiguredSampler() = default;

Sampler* ConfiguredSampler::GetSampler() const {
  return sampler_.get();
}

const std::string& ConfiguredSampler::GetEnableSettingPath() const {
  return enable_setting_path_;
}

bool ConfiguredSampler::GetSettingEnabledDefaultValue() const {
  return setting_enabled_default_value_;
}

}  // namespace reporting
