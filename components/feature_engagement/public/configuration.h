// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_

#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace feature_engagement {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ConfigurationProvider;
#endif

// Max number of days for storing client side event data, ~10 years.
constexpr uint32_t kMaxStoragePeriod = 365 * 10;

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

  // Store client side data related to events for this minimum this long,
  // see the `kMaxStoragePeriod` constant for the max supported value.
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
  std::optional<std::vector<std::string>> affected_features;
};

bool operator==(const SessionRateImpact& lhs, const SessionRateImpact& rhs);
std::ostream& operator<<(std::ostream& os, const SessionRateImpact& impact);

// BlockedBy describes which features the |blocked_by| of a given
// FeatureConfig should affect. It can affect either |ALL| (default), |NONE|,
// or an |EXPLICIT| list of the features. In the latter case, a list of affected
// features is given as their base::Feature name.
struct BlockedBy {
 public:
  enum class Type {
    ALL = 0,      // Affects all other features.
    NONE = 1,     // Affects no other features.
    EXPLICIT = 2  // Affects only features in |affected_features|.
  };

  BlockedBy();
  BlockedBy(const BlockedBy& other);
  ~BlockedBy();

  // Describes which features are impacted.
  Type type{Type::ALL};

  // In the case of the Type |EXPLICIT|, this is the list of affected
  // base::Feature names.
  std::optional<std::vector<std::string>> affected_features;
};

bool operator==(const BlockedBy& lhs, const BlockedBy& rhs);
std::ostream& operator<<(std::ostream& os, const BlockedBy& impact);

// Blocking describes which features the |blocking| of a given FeatureConfig
// should affect. It can affect either |ALL| (default) or |NONE|.
struct Blocking {
 public:
  enum class Type {
    ALL = 0,   // Affects all other features.
    NONE = 1,  // Affects no other features.
  };

  Blocking();
  Blocking(const Blocking& other);
  ~Blocking();

  // Describes which features are impacted.
  Type type{Type::ALL};
};

bool operator==(const Blocking& lhs, const Blocking& rhs);
std::ostream& operator<<(std::ostream& os, const Blocking& impact);

// A SnoozeParams describes the parameters for snoozable options of in-product
// help.
struct SnoozeParams {
 public:
  // The maximum number of times an in-product-help can be snoozed.
  uint32_t max_limit{0};
  // The minimum time interval between snoozes.
  uint32_t snooze_interval{0};

  SnoozeParams();
  SnoozeParams(const SnoozeParams& other);
  ~SnoozeParams();
};

bool operator==(const SnoozeParams& lhs, const SnoozeParams& rhs);
std::ostream& operator<<(std::ostream& os, const SnoozeParams& impact);

// A FeatureConfig contains all the configuration for a given feature.
struct FeatureConfig {
 public:
  FeatureConfig();
  FeatureConfig(const FeatureConfig& other);
  ~FeatureConfig();

  // Whether the configuration has been successfully parsed.
  bool valid = false;

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

  // Which features the current in-product help is blocked by.
  BlockedBy blocked_by;

  // Which features the current in-product help is blocking.
  Blocking blocking;

  // Number of days the in-product help has been available must fit this
  // comparison.
  Comparator availability;

  // Whether this configuration will only be used for tracking and comparisons
  // between experiment groups. Setting this to true will ensure that
  // Tracker::ShouldTriggerHelpUI(...) always returns false, but if all
  // other conditions are met, it will still be recorded as having been
  // shown in the internal database and through UMA.
  bool tracking_only{false};

  // Snoozing parameter to decide if in-product help should be shown.
  SnoozeParams snooze_params;

  // Groups this feature is part of.
  std::vector<std::string> groups;
};

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs);
std::ostream& operator<<(std::ostream& os, const FeatureConfig& feature_config);

// A GroupConfig contains all the configuration for a given group.
struct GroupConfig {
 public:
  GroupConfig();
  GroupConfig(const GroupConfig& other);
  ~GroupConfig();

  // Whether the group configuration has been successfully parsed.
  bool valid{false};

  // For each feature in this group, the number of in-product help triggered
  // within this session must fit this comparison (in addition to any
  // |session_rate| from that feature itself).
  Comparator session_rate;

  // The configuration for a particular event that will be searched for when
  // counting how many times in-product help has been triggered for features in
  // this particular group.
  EventConfig trigger;

  // A set of all event configurations for this group.
  std::set<EventConfig> event_configs;
};

bool operator==(const GroupConfig& lhs, const GroupConfig& rhs);
std::ostream& operator<<(std::ostream& os, const GroupConfig& feature_config);

// A Configuration contains the current set of runtime configurations.
// It is up to each implementation of Configuration to provide a way to
// register features and their configurations.
class Configuration {
 public:
  // Convenience aliases for typical implementations of Configuration.
  using ConfigMap = std::map<std::string, FeatureConfig>;
  using GroupConfigMap = std::map<std::string, GroupConfig>;
  using EventPrefixSet = std::unordered_set<std::string>;

  Configuration(const Configuration&) = delete;
  Configuration& operator=(const Configuration&) = delete;

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

  // Returns the list of the names of all registered features.
  virtual const std::vector<std::string> GetRegisteredFeatures() const = 0;

  // Returns the GroupConfig for the given |group|. The |group| must
  // be registered with the Configuration instance.
  virtual const GroupConfig& GetGroupConfig(
      const base::Feature& group) const = 0;

  // Returns the GroupConfig for the given |group_name|. The |group_name| must
  // be registered with the Configuration instance.
  virtual const GroupConfig& GetGroupConfigByName(
      const std::string& group_name) const = 0;

  // Returns the immutable GroupConfigMap that contains all registered groups.
  virtual const GroupConfigMap& GetRegisteredGroupConfigs() const = 0;

  // Returns the list of the names of all registered groups.
  virtual const std::vector<std::string> GetRegisteredGroups() const = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Updates the config of a specific feature. The new config will replace the
  // existing cofig.
  virtual void UpdateConfig(const base::Feature& feature,
                            const ConfigurationProvider* provider) = 0;

  // Returns the allowed set of prefixes for the events which can be stored and
  // kept, regardless of whether or not they are used in a config.
  virtual const EventPrefixSet& GetRegisteredAllowedEventPrefixes() const = 0;
#endif

 protected:
  Configuration() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_CONFIGURATION_H_
