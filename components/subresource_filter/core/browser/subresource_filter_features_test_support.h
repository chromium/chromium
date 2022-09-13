// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_

#include <iosfwd>
#include <vector>

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

namespace subresource_filter {
namespace testing {

// Helper class to override the active subresource filtering configuration to be
// used in tests while the instance is in scope.
//
// Configuration overrides can be nested, and will take effect regardless of
// field trial, feature, and/or variation parameter states.
class ScopedSubresourceFilterConfigurator {
 public:
  explicit ScopedSubresourceFilterConfigurator(
      scoped_refptr<ConfigurationList> config_list = nullptr);
  explicit ScopedSubresourceFilterConfigurator(Configuration config);
  explicit ScopedSubresourceFilterConfigurator(
      std::vector<Configuration> configs);

  ScopedSubresourceFilterConfigurator(
      const ScopedSubresourceFilterConfigurator&) = delete;
  ScopedSubresourceFilterConfigurator& operator=(
      const ScopedSubresourceFilterConfigurator&) = delete;

  ~ScopedSubresourceFilterConfigurator();

  void ResetConfiguration(Configuration config);
  void ResetConfiguration(std::vector<Configuration> config);

 private:
  void ResetConfiguration(
      scoped_refptr<ConfigurationList> config_list = nullptr);

  scoped_refptr<ConfigurationList> original_config_;
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_
