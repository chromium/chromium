// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace subresource_filter {
namespace testing {

// ScopedSubresourceFilterConfigurator ----------------------------------------

ScopedSubresourceFilterConfigurator::ScopedSubresourceFilterConfigurator(
    scoped_refptr<ConfigurationList> configs_list)
    : original_config_(GetAndSetActivateConfigurations(configs_list)) {}

ScopedSubresourceFilterConfigurator::ScopedSubresourceFilterConfigurator(
    Configuration config)
    : ScopedSubresourceFilterConfigurator(
          std::vector<Configuration>(1, std::move(config))) {}

ScopedSubresourceFilterConfigurator::ScopedSubresourceFilterConfigurator(
    std::vector<Configuration> configs)
    : ScopedSubresourceFilterConfigurator(
          base::MakeRefCounted<ConfigurationList>(std::move(configs))) {}

ScopedSubresourceFilterConfigurator::~ScopedSubresourceFilterConfigurator() {
  GetAndSetActivateConfigurations(std::move(original_config_));
}

void ScopedSubresourceFilterConfigurator::ResetConfiguration(
    scoped_refptr<ConfigurationList> configs_list) {
  GetAndSetActivateConfigurations(configs_list);
}

void ScopedSubresourceFilterConfigurator::ResetConfiguration(
    Configuration config) {
  ResetConfiguration(std::vector<Configuration>(1, std::move(config)));
}

void ScopedSubresourceFilterConfigurator::ResetConfiguration(
    std::vector<Configuration> config) {
  ResetConfiguration(
      base::MakeRefCounted<ConfigurationList>(std::move(config)));
}

}  // namespace testing
}  // namespace subresource_filter
