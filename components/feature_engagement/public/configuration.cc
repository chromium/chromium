// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/configuration.h"

#include <string>

#include "base/logging.h"
#include "base/optional.h"

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
      NOTREACHED();
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
      NOTREACHED();
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
      NOTREACHED();
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

FeatureConfig::FeatureConfig() : valid(false), tracking_only(false) {}

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

}  // namespace feature_engagement
