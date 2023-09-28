// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_
#define COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_

#include <type_traits>
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/types/always_false.h"

namespace miracle_parameter {

const int kMiracleParameterMemory512MB = 512;
const int kMiracleParameterMemory1GB = 1024;
const int kMiracleParameterMemory2GB = 2 * 1024;
const int kMiracleParameterMemory4GB = 4 * 1024;
const int kMiracleParameterMemory8GB = 8 * 1024;
const int kMiracleParameterMemory16GB = 16 * 1024;

// Base class for MiracleParameter.
template <typename T>
class MiracleParameterBase {
 public:
  constexpr MiracleParameterBase(const base::Feature* feature,
                                 const char* param_name,
                                 T default_value)
      : feature_(feature),
        param_name_(param_name),
        default_value_(default_value) {}

  virtual T Get() const = 0;

 protected:
  std::string GetNameWithSuffix() const {
    int physical_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();

    const char* suffix =
        physical_memory_mb < kMiracleParameterMemory512MB  ? "ForLessThan512MB"
        : physical_memory_mb < kMiracleParameterMemory1GB  ? "For512MBTo1GB"
        : physical_memory_mb < kMiracleParameterMemory2GB  ? "For1GBTo2GB"
        : physical_memory_mb < kMiracleParameterMemory4GB  ? "For2GBTo4GB"
        : physical_memory_mb < kMiracleParameterMemory8GB  ? "For4GBTo8GB"
        : physical_memory_mb < kMiracleParameterMemory16GB ? "For8GBTo16GB"
                                                           : "For16GBAndAbove";

    return base::StrCat({param_name_, suffix});
  }

  // This field is not a raw_ptr<> because this class is used with constexpr.
  RAW_PTR_EXCLUSION const base::Feature* const feature_;
  const char* const param_name_;
  const T default_value_;
};

// Shared declaration for various MiracleParameter<T> types.
//
// This template is defined for the following types T:
//   bool
//   int
//   double
//   std::string
//   enum types
//   TimeDelta
//
// Unlike FeatureParam, MiracleParameter determines the parameter value based on
// the amount of physical memory when the following suffix is used as a
// parameter name. If there are no parameter settings that have the following
// suffixes, MiracleParameter behaves the same as the FeatureParam.
//
// - "ForLessThan512MB" for less than 512MB memory devices.
// - "For512MBTo1GB" for 512MB to 1GB memory devices.
// - "For1GBTo2GB" for 1GB to 2GB memory devices.
// - "For2GBTo4GB" for 2GB to 4GB memory devices.
// - "For4GBTo8GB" for 4GB to 8GB memory devices.
// - "For8GBTo16GB" for 8GB to 16GB memory devices.
// - "For16GBAndAbove" for 16GB memory and above devices.
template <typename T, bool IsEnum = std::is_enum_v<T>>
class MiracleParameter {
 public:
  // Prevent use of MiracleParameter<> with unsupported types (e.g. void*). Uses
  // T in its definition so that evaluation is deferred until the template is
  // instantiated.
  static_assert(base::AlwaysFalse<T>, "unsupported MiracleParameter<> type");
};

// Provides a similar feature with FeatureParam<std::string> except the return
// value is determined by the amount of physical memory.
template <>
class MiracleParameter<std::string> : public MiracleParameterBase<std::string> {
 public:
  constexpr MiracleParameter(const base::Feature* feature,
                             const char* param_name,
                             std::string default_value)
      : MiracleParameterBase(feature, param_name, std::move(default_value)) {}

  std::string Get() const override {
    const std::string value =
        base::GetFieldTrialParamValueByFeature(*feature_, GetNameWithSuffix());

    if (!value.empty()) {
      return value;
    }

    // If there are no memory dependent parameter settings, MiracleParameter
    // behaves the same as the FeatureParam.
    const std::string fallback_value =
        base::GetFieldTrialParamValueByFeature(*feature_, param_name_);
    return fallback_value.empty() ? default_value_ : fallback_value;
  }
};

// Provides a similar feature with FeatureParam<double> except the return value
// is determined by the amount of physical memory.
template <>
class MiracleParameter<double> : public MiracleParameterBase<double> {
 public:
  constexpr MiracleParameter(const base::Feature* feature,
                             const char* param_name,
                             double default_value)
      : MiracleParameterBase(feature, param_name, default_value) {}

  double Get() const override {
    return base::GetFieldTrialParamByFeatureAsDouble(
        *feature_, GetNameWithSuffix(),
        base::GetFieldTrialParamByFeatureAsDouble(*feature_, param_name_,
                                                  default_value_));
  }
};

// Provides a similar feature with FeatureParam<int> except the return value is
// determined by the amount of physical memory.
template <>
class MiracleParameter<int> : public MiracleParameterBase<int> {
 public:
  constexpr MiracleParameter(const base::Feature* feature,
                             const char* param_name,
                             int default_value)
      : MiracleParameterBase(feature, param_name, default_value) {}

  int Get() const override {
    return base::GetFieldTrialParamByFeatureAsInt(
        *feature_, GetNameWithSuffix(),
        base::GetFieldTrialParamByFeatureAsInt(*feature_, param_name_,
                                               default_value_));
  }
};

// Provides a similar feature with FeatureParam<bool> except the return value is
// determined by the amount of physical memory.
template <>
class MiracleParameter<bool> : public MiracleParameterBase<bool> {
 public:
  constexpr MiracleParameter(const base::Feature* feature,
                             const char* param_name,
                             bool default_value)
      : MiracleParameterBase(feature, param_name, default_value) {}

  bool Get() const override {
    return base::GetFieldTrialParamByFeatureAsBool(
        *feature_, GetNameWithSuffix(),
        base::GetFieldTrialParamByFeatureAsBool(*feature_, param_name_,
                                                default_value_));
  }
};

// Provides a similar feature with FeatureParam<TimeDelta> except the return
// value is determined by the amount of physical memory.
template <>
class MiracleParameter<base::TimeDelta>
    : public MiracleParameterBase<base::TimeDelta> {
 public:
  constexpr MiracleParameter(const base::Feature* feature,
                             const char* param_name,
                             base::TimeDelta default_value)
      : MiracleParameterBase(feature, param_name, std::move(default_value)) {}

  base::TimeDelta Get() const override {
    return base::GetFieldTrialParamByFeatureAsTimeDelta(
        *feature_, GetNameWithSuffix(),
        base::GetFieldTrialParamByFeatureAsTimeDelta(*feature_, param_name_,
                                                     default_value_));
  }
};

// Provides a similar feature with FeatureParam<Enum> except the return value is
// determined by the amount of physical memory.
template <typename Enum>
class MiracleParameter<Enum, true> : public MiracleParameterBase<Enum> {
 public:
  constexpr MiracleParameter(
      const base::Feature* feature,
      const char* param_name,
      const Enum default_value,
      const base::span<const typename base::FeatureParam<Enum>::Option> options)
      : MiracleParameterBase<Enum>(feature, param_name, default_value),
        options_(std::move(options)) {}

  Enum Get() const override {
    return GetFieldTrialParamByFeatureAsEnum(
        *this->feature_, this->GetNameWithSuffix(),
        GetFieldTrialParamByFeatureAsEnum(*this->feature_, this->param_name_,
                                          this->default_value_));
  }

 private:
  Enum GetFieldTrialParamByFeatureAsEnum(const base::Feature& feature,
                                         const std::string& param_name,
                                         const Enum default_value) const {
    std::string string_value =
        base::GetFieldTrialParamValueByFeature(feature, param_name);
    if (string_value.empty()) {
      return default_value;
    }

    for (const auto& option : options_) {
      if (string_value == option.name) {
        return option.value;
      }
    }

    base::LogInvalidEnumValue(feature, param_name, string_value,
                              static_cast<int>(default_value));
    return default_value;
  }

  const base::span<const typename base::FeatureParam<Enum>::Option> options_;
};

}  // namespace miracle_parameter

#endif  // COMPONENTS_MIRACLE_PARAMETER_COMMON_PUBLIC_MIRACLE_PARAMETER_H_
