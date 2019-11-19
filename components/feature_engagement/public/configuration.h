// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"

namespace base {
struct Feature;
}

namespace feature_engagement {

// A ComparatorType describes the relationship between two numbers.
enum ComparatorType {
  ANY = 0,  // Will always yield true.
  LESS_THAN = 1,
  GREATER_THAN = 2,
  LESS_THAN_OR_EQUAL = 3,
  GREATER_THAN_OR_EQUAL = 4,
  EQUAL = 5,
  NOT_EQUAL = 6,
};

// A Comparator provides a way of comparing a uint32_t another uint32_t and
// verifying their relationship.
struct Comparator {
 public:
  Comparator();
  Comparator(ComparatorType type, uint32_t value);
  ~Comparator();

  // Returns true if the |v| meets the this criteria based on the current
  // |type| and |value|.
  bool MeetsCriteria(uint32_t v) const;

  ComparatorType type;
  uint32_t value;
};

bool operator==(const Comparator& lhs, const Comparator& rhs);
bool operator<(const Comparator& lhs, const Comparator& rhs);
std::ostream& operator<<(std::ostream& os, const Comparator& comparator);

// A EventConfig contains all the information about how many times
// a particular event should or should not have triggered, for which window
// to search in and for how long to store it.
struct EventConfig {
 public:
  EventConfig();
  EventConfig(const std::string& name,
              Comparator comparator,
              uint32_t window,
              uint32_t storage);
  ~EventConfig();

  // The identifier of the event.
  std::string name;

  // The number of events it is required to find within the search window.
  Comparator comparator;

  // Search for this event within this window.
  uint32_t window;

  // Store client side data related to events for this minimum this long.
  uint32_t storage;
};

bool operator==(const EventConfig& lhs, const EventConfig& rhs);
bool operator!=(const EventConfig& lhs, const EventConfig& rhs);
bool operator<(const EventConfig& lhs, const EventConfig& rhs);
std::ostream& operator<<(std::ostream& os, const EventConfig& event_config);

// A SessionRateImpact describes which features the |session_rate| of a given
// FeatureConfig should affect. It can affect either |ALL| (default), |NONE|,
// or an |EXPLICIT| list of the features. In the latter case, a list of affected
// features is given as their base::Feature name.
struct SessionRateImpact {
 public:
  enum class Type {
    ALL = 0,      // Affects all other features.
    NONE = 1,     // Affects no other features.
    EXPLICIT = 2  // Affects only features in |affected_features|.
  };

  SessionRateImpact();
  SessionRateImpact(const SessionRateImpact& other);
  ~SessionRateImpact();

  // Describes which features are impacted.
  Type type;

  // In the case of the Type |EXPLICIT|, this is the list of affected
  // base::Feature names.
  base::Optional<std::vector<std::string>> affected_features;
};

bool operator==(const SessionRateImpact& lhs, const SessionRateImpact& rhs);
std::ostream& operator<<(std::ostream& os, const SessionRateImpact& impact);

// A FeatureConfig contains all the configuration for a given feature.
struct FeatureConfig {
 public:
  FeatureConfig();
  FeatureConfig(const FeatureConfig& other);
  ~FeatureConfig();

  // Whether the configuration has been successfully parsed.
  bool valid;

  // The configuration for a particular event that will be searched for when
  // counting how many times a particular feature has been used.
  EventConfig used;

  // The configuration for a particular event that will be searched for when
  // counting how many times in-product help has been triggered for a particular
  // feature.
  EventConfig trigger;

  // A set of all event configurations.
  std::set<EventConfig> event_configs;

  // Number of in-product help triggered within this session must fit this
  // comparison.
  Comparator session_rate;

  // Which features the showing this in-product help impacts.
  SessionRateImpact session_rate_impact;

  // Number of days the in-product help has been available must fit this
  // comparison.
  Comparator availability;

  // Whether this configuration will only be used for tracking and comparisons
  // between experiment groups. Setting this to true will ensure that
  // Tracker::ShouldTriggerHelpUI(...) always returns false, but if all
  // other conditions are met, it will still be recorded as having been
  // shown in the internal database and through UMA.
  bool tracking_only;
};

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs);
std::ostream& operator<<(std::ostream& os, const FeatureConfig& feature_config);

// A Configuration contains the current set of runtime configurations.
// It is up to each implementation of Configuration to provide a way to
// register features and their configurations.
class Configuration {
 public:
  // Convenience alias for typical implementations of Configuration.
  using ConfigMap = std::map<std::string, FeatureConfig>;

  virtual ~Configuration() = default;

  // Returns the FeatureConfig for the given |feature|. The |feature| must
  // be registered with the Configuration instance.
  virtual const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const = 0;

  // Returns the FeatureConfig for the given |feature|. The |feature_name| must
  // be registered with the Configuration instance.
  virtual const FeatureConfig& GetFeatureConfigByName(
      const std::string& feature_name) const = 0;

  // Returns the immutable ConfigMap that contains all registered features.
  virtual const ConfigMap& GetRegisteredFeatureConfigs() const = 0;

  // Returns the list of the names of all registred features.
  virtual const std::vector<std::string> GetRegisteredFeatures() const = 0;

 protected:
  Configuration() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_
