// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/configuration.h"

#include <string>

#include "base/notreached.h"

namespace feature_engagement {
namespace {
std::ostream& operator<<(std::ostream& os, const SessionRateImpact::Type type) {
  switch (type) {
    case SessionRateImpact::Type::ALL:
      return os << "ALL";
    case SessionRateImpact::Type::NONE:
      return os << "NONE";
    case SessionRateImpact::Type::EXPLICIT:
      return os << "EXPLICIT";
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
      return os;
  }
}

std::ostream& operator<<(std::ostream& os, BlockedBy::Type type) {
  switch (type) {
    case BlockedBy::Type::ALL:
      return os << "ALL";
    case BlockedBy::Type::NONE:
      return os << "NONE";
    case BlockedBy::Type::EXPLICIT:
      return os << "EXPLICIT";
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
      return os;
  }
}

std::ostream& operator<<(std::ostream& os, Blocking::Type type) {
  switch (type) {
    case Blocking::Type::ALL:
      return os << "ALL";
    case Blocking::Type::NONE:
      return os << "NONE";
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
      return os;
  }
}

}  // namespace

Comparator::Comparator() : type(ANY), value(0) {}

Comparator::Comparator(ComparatorType type, uint32_t value)
    : type(type), value(value) {}

Comparator::~Comparator() = default;

bool Comparator::MeetsCriteria(uint32_t v) const {
  switch (type) {
    case ANY:
      return true;
    case LESS_THAN:
      return v < value;
    case GREATER_THAN:
      return v > value;
    case LESS_THAN_OR_EQUAL:
      return v <= value;
    case GREATER_THAN_OR_EQUAL:
      return v >= value;
    case EQUAL:
      return v == value;
    case NOT_EQUAL:
      return v != value;
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

std::ostream& operator<<(std::ostream& os, const Comparator& comparator) {
  switch (comparator.type) {
    case ANY:
      return os << "ANY";
    case LESS_THAN:
      return os << "<" << comparator.value;
    case GREATER_THAN:
      return os << ">" << comparator.value;
    case LESS_THAN_OR_EQUAL:
      return os << "<=" << comparator.value;
    case GREATER_THAN_OR_EQUAL:
      return os << ">=" << comparator.value;
    case EQUAL:
      return os << "==" << comparator.value;
    case NOT_EQUAL:
      return os << "!=" << comparator.value;
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
      return os;
  }
}

EventConfig::EventConfig() : window(0), storage(0) {}

EventConfig::EventConfig(const std::string& name,
                         Comparator comparator,
                         uint32_t window,
                         uint32_t storage)
    : name(name), comparator(comparator), window(window), storage(storage) {}

EventConfig::~EventConfig() = default;

std::ostream& operator<<(std::ostream& os, const EventConfig& event_config) {
  return os << "{ name: " << event_config.name
            << ", comparator: " << event_config.comparator
            << ", window: " << event_config.window
            << ", storage: " << event_config.storage << " }";
}

SessionRateImpact::SessionRateImpact() : type(SessionRateImpact::Type::ALL) {}

SessionRateImpact::SessionRateImpact(const SessionRateImpact& other) = default;

SessionRateImpact::~SessionRateImpact() = default;

BlockedBy::BlockedBy() = default;

BlockedBy::BlockedBy(const BlockedBy& other) = default;

BlockedBy::~BlockedBy() = default;

Blocking::Blocking() = default;

Blocking::Blocking(const Blocking& other) = default;

Blocking::~Blocking() = default;

SnoozeParams::SnoozeParams() = default;

SnoozeParams::SnoozeParams(const SnoozeParams& other) = default;

SnoozeParams::~SnoozeParams() = default;

std::ostream& operator<<(std::ostream& os, const BlockedBy& blocked_by) {
  os << "{ type: " << blocked_by.type << ", affected_features: ";
  if (!blocked_by.affected_features.has_value()) {
    return os << "NO VALUE }";
  }

  os << "[";
  bool first = true;
  for (const auto& affected_feature : blocked_by.affected_features.value()) {
    if (first) {
      first = false;
      os << affected_feature;
    } else {
      os << ", " << affected_feature;
    }
  }
  return os << "] }";
}

std::ostream& operator<<(std::ostream& os, const Blocking& blocking) {
  return os << "{ type: " << blocking.type << " }";
}

std::ostream& operator<<(std::ostream& os, const SnoozeParams& snooze_params) {
  return os << "{ max_limit: " << snooze_params.max_limit
            << ", snooze_interval: " << snooze_params.snooze_interval << ", }";
}

std::ostream& operator<<(std::ostream& os, const SessionRateImpact& impact) {
  os << "{ type: " << impact.type << ", affected_features: ";
  if (!impact.affected_features.has_value())
    return os << "NO VALUE }";

  os << "[";
  bool first = true;
  for (const auto& affected_feature : impact.affected_features.value()) {
    if (first) {
      first = false;
      os << affected_feature;
    } else {
      os << ", " << affected_feature;
    }
  }
  return os << "] }";
}

bool operator==(const SessionRateImpact& lhs, const SessionRateImpact& rhs) {
  return std::tie(lhs.type, lhs.affected_features) ==
         std::tie(rhs.type, rhs.affected_features);
}

bool operator==(const BlockedBy& lhs, const BlockedBy& rhs) {
  return std::tie(lhs.type, lhs.affected_features) ==
         std::tie(rhs.type, rhs.affected_features);
}

bool operator==(const Blocking& lhs, const Blocking& rhs) {
  return lhs.type == rhs.type;
}

bool operator==(const SnoozeParams& lhs, const SnoozeParams& rhs) {
  return std::tie(lhs.max_limit, lhs.snooze_interval) ==
         std::tie(rhs.max_limit, rhs.snooze_interval);
}

FeatureConfig::FeatureConfig() = default;

FeatureConfig::FeatureConfig(const FeatureConfig& other) = default;

FeatureConfig::~FeatureConfig() = default;

bool operator==(const Comparator& lhs, const Comparator& rhs) {
  return std::tie(lhs.type, lhs.value) == std::tie(rhs.type, rhs.value);
}

bool operator<(const Comparator& lhs, const Comparator& rhs) {
  return std::tie(lhs.type, lhs.value) < std::tie(rhs.type, rhs.value);
}

bool operator==(const EventConfig& lhs, const EventConfig& rhs) {
  return std::tie(lhs.name, lhs.comparator, lhs.window, lhs.storage) ==
         std::tie(rhs.name, rhs.comparator, rhs.window, rhs.storage);
}

bool operator!=(const EventConfig& lhs, const EventConfig& rhs) {
  return !(lhs == rhs);
}

bool operator<(const EventConfig& lhs, const EventConfig& rhs) {
  return std::tie(lhs.name, lhs.comparator, lhs.window, lhs.storage) <
         std::tie(rhs.name, rhs.comparator, rhs.window, rhs.storage);
}

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs) {
  return std::tie(lhs.valid, lhs.used, lhs.trigger, lhs.event_configs,
                  lhs.session_rate, lhs.availability) ==
         std::tie(rhs.valid, rhs.used, rhs.trigger, rhs.event_configs,
                  rhs.session_rate, rhs.availability);
}

std::ostream& operator<<(std::ostream& os,
                         const FeatureConfig& feature_config) {
  os << "{ valid: " << feature_config.valid << ", used: " << feature_config.used
     << ", trigger: " << feature_config.trigger << ", event_configs: [";
  bool first = true;
  for (const auto& event_config : feature_config.event_configs) {
    if (first) {
      first = false;
      os << event_config;
    } else {
      os << ", " << event_config;
    }
  }
  return os << "], session_rate: " << feature_config.session_rate
            << ", availability: " << feature_config.availability << " }";
}

GroupConfig::GroupConfig() = default;

GroupConfig::GroupConfig(const GroupConfig& other) = default;

GroupConfig::~GroupConfig() = default;

bool operator==(const GroupConfig& lhs, const GroupConfig& rhs) {
  return std::tie(lhs.valid, lhs.trigger, lhs.event_configs,
                  lhs.session_rate) ==
         std::tie(rhs.valid, rhs.trigger, rhs.event_configs, rhs.session_rate);
}

std::ostream& operator<<(std::ostream& os, const GroupConfig& group_config) {
  os << "{ valid: " << group_config.valid
     << ", trigger: " << group_config.trigger << ", event_configs: [";
  bool first = true;
  for (const auto& event_config : group_config.event_configs) {
    if (first) {
      first = false;
      os << event_config;
    } else {
      os << ", " << event_config;
    }
  }
  return os << "], session_rate: " << group_config.session_rate << " }";
}

}  // namespace feature_engagement
