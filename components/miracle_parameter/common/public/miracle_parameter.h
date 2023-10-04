// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_
#define COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace miracle_parameter {

namespace {

template <typename Enum>
Enum GetFieldTrialParamByFeatureAsEnum(
    const base::Feature& feature,
    const std::string& param_name,
    const Enum default_value,
    const base::span<const typename base::FeatureParam<Enum>::Option>&
        options) {
  std::string string_value =
      base::GetFieldTrialParamValueByFeature(feature, param_name);
  if (string_value.empty()) {
    return default_value;
  }

  for (const auto& option : options) {
    if (string_value == option.name) {
      return option.value;
    }
  }

  base::LogInvalidEnumValue(feature, param_name, string_value,
                            static_cast<int>(default_value));
  return default_value;
}

}  // namespace

constexpr int kMiracleParameterMemory512MB = 512;
constexpr int kMiracleParameterMemory1GB = 1024;
constexpr int kMiracleParameterMemory2GB = 2 * 1024;
constexpr int kMiracleParameterMemory4GB = 4 * 1024;
constexpr int kMiracleParameterMemory8GB = 8 * 1024;
constexpr int kMiracleParameterMemory16GB = 16 * 1024;

// GetParamNameWithSuffix put a parameter name suffix based on
// the amount of physical memory.
//
// - "ForLessThan512MB" for less than 512MB memory devices.
// - "For512MBTo1GB" for 512MB to 1GB memory devices.
// - "For1GBTo2GB" for 1GB to 2GB memory devices.
// - "For2GBTo4GB" for 2GB to 4GB memory devices.
// - "For4GBTo8GB" for 4GB to 8GB memory devices.
// - "For8GBTo16GB" for 8GB to 16GB memory devices.
// - "For16GBAndAbove" for 16GB memory and above devices.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
std::string GetParamNameWithSuffix(const std::string& param_name);

// Provides a similar behavior with FeatureParam<std::string> except the return
// value is determined by the amount of physical memory.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
std::string GetMiracleParameterAsString(const base::Feature& feature,
                                        const std::string& param_name,
                                        const std::string& default_value);

// Provides a similar behavior with FeatureParam<double> except the return value
// is determined by the amount of physical memory.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
double GetMiracleParameterAsDouble(const base::Feature& feature,
                                   const std::string& param_name,
                                   double default_value);

// Provides a similar behavior with FeatureParam<int> except the return value is
// determined by the amount of physical memory.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
int GetMiracleParameterAsInt(const base::Feature& feature,
                             const std::string& param_name,
                             int default_value);

// Provides a similar behavior with FeatureParam<bool> except the return value
// is determined by the amount of physical memory.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
bool GetMiracleParameterAsBool(const base::Feature& feature,
                               const std::string& param_name,
                               bool default_value);

// Provides a similar behavior with FeatureParam<base::TimeDelta> except the
// return value is determined by the amount of physical memory.
COMPONENT_EXPORT(MIRACLE_PARAMETER)
base::TimeDelta GetMiracleParameterAsTimeDelta(const base::Feature& feature,
                                               const std::string& param_name,
                                               base::TimeDelta default_value);

// Provides a similar behavior with FeatureParam<Enum> except the return value
// is determined by the amount of physical memory.
template <typename Enum>
Enum GetMiracleParameterAsEnum(
    const base::Feature& feature,
    const std::string& param_name,
    const Enum default_value,
    const base::span<const typename base::FeatureParam<Enum>::Option> options) {
  return GetFieldTrialParamByFeatureAsEnum(
      feature, GetParamNameWithSuffix(param_name),
      GetFieldTrialParamByFeatureAsEnum(feature, param_name, default_value,
                                        options),
      options);
}

#define MIRACLE_PARAMETER_FOR_STRING(function_name, feature, param_name,    \
                                     default_value)                         \
  std::string function_name() {                                             \
    static const std::string value =                                        \
        miracle_parameter::GetMiracleParameterAsString(feature, param_name, \
                                                       default_value);      \
    return value;                                                           \
  }

#define MIRACLE_PARAMETER_FOR_DOUBLE(function_name, feature, param_name,    \
                                     default_value)                         \
  double function_name() {                                                  \
    static const double value =                                             \
        miracle_parameter::GetMiracleParameterAsDouble(feature, param_name, \
                                                       default_value);      \
    return value;                                                           \
  }

#define MIRACLE_PARAMETER_FOR_INT(function_name, feature, param_name,     \
                                  default_value)                          \
  int function_name() {                                                   \
    static const int value = miracle_parameter::GetMiracleParameterAsInt( \
        feature, param_name, default_value);                              \
    return value;                                                         \
  }

#define MIRACLE_PARAMETER_FOR_BOOL(function_name, feature, param_name,      \
                                   default_value)                           \
  bool function_name() {                                                    \
    static const bool value = miracle_parameter::GetMiracleParameterAsBool( \
        feature, param_name, default_value);                                \
    return value;                                                           \
  }

#define MIRACLE_PARAMETER_FOR_TIME_DELTA(function_name, feature, param_name,   \
                                         default_value)                        \
  base::TimeDelta function_name() {                                            \
    static const base::TimeDelta value =                                       \
        miracle_parameter::GetMiracleParameterAsTimeDelta(feature, param_name, \
                                                          default_value);      \
    return value;                                                              \
  }

#define MIRACLE_PARAMETER_FOR_ENUM(function_name, feature, param_name,      \
                                   default_value, type, options)            \
  type function_name() {                                                    \
    static const type value = miracle_parameter::GetMiracleParameterAsEnum( \
        feature, param_name, default_value, base::make_span(options));      \
    return value;                                                           \
  }

}  // namespace miracle_parameter

#endif  // COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_
