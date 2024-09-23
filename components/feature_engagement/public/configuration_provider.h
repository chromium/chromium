// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_PROVIDER_H_

#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/feature_engagement/public/stats.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {

struct FeatureConfig;
struct GroupConfig;

// Provides configuration for Feature Engagement Features and Groups.
class ConfigurationProvider {
 public:
  ConfigurationProvider();
  virtual ~ConfigurationProvider();
  ConfigurationProvider(const ConfigurationProvider&) = delete;
  void operator=(const ConfigurationProvider&) = delete;

  // Determines whether to update `config`; returns whether `config` is
  // modified. Default behavior is to return false (override in implementation
  // classes).
  //
  // The configuration may be default-constructed to begin with, or it may
  // already contain data from a previous provider in the chain; a provider may
  // choose to override previously-loaded data, or skip entries that are already
  // valid.
  //
  // Note that `config.valid` may still not be true after a call, even if the
  // method returns true. It is possible that data has been read, but is
  // invalid.
  //
  // Also note that there is no need to de-dup or flatten features and groups;
  // this is done as a post-processing step by the tracker implementation code.
  // Group and feature names may be checked for validity; `ContainsFeature`
  // below is a convenience method to check a name against `known_features` or
  // `known_groups`.
  virtual bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      FeatureConfig& config,
      const FeatureVector& known_features,
      const GroupVector& known_groups) const;

  // As `MaybeProvideFeatureConfiguration()`, but reads a group config.
  virtual bool MaybeProvideGroupConfiguration(const base::Feature& feature,
                                              GroupConfig& config) const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Provides an allowed set of prefixes for the events which can be stored and
  // kept, regardless of whether or not they are used in a config.
  virtual std::set<std::string> MaybeProvideAllowedEventPrefixes(
      const base::Feature& feature) const;
#endif

  // Gets a description of the source of the configuration for debugging and
  // error tracing purposes.
  virtual const char* GetConfigurationSourceDescription() const = 0;

  // Gets the event to record when the configuration is successfully read.
  // Defaults to SUCCESS, but for historical reasons, some providers may need to
  // return a different value.
  virtual feature_engagement::stats::ConfigParsingEvent GetOnSuccessEvent()
      const;
};

using ConfigurationProviderList =
    std::vector<std::unique_ptr<ConfigurationProvider>>;

// Used to check whether Feature with `feature_name` is present in
// `feature_list`; works with both FeatureVector and GroupVector.
template <typename T>
static bool ContainsFeature(std::string_view feature_name,
                            const T& feature_list) {
  const auto it = std::find_if(feature_list.begin(), feature_list.end(),
                               [&feature_name](const base::Feature* f) {
                                 return f->name == feature_name;
                               });
  return it != feature_list.end();
}

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_PROVIDER_H_
