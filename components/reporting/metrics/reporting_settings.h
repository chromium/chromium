// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_REPORTING_SETTINGS_H_
#define COMPONENTS_REPORTING_METRICS_REPORTING_SETTINGS_H_

#include <string>

#include "base/callback_list.h"
#include "base/values.h"

namespace reporting {

// Base class for settings retreivers. It is built mainly to be implemented by
// wrappers of `ash::CrosSettings` but can also be implemented by other
// settings providers if needed.
class ReportingSettings {
 public:
  virtual ~ReportingSettings() = default;

  // Add an observer Callback for changes for the given `path`.
  virtual base::CallbackListSubscription AddSettingsObserver(
      const std::string& path,
      base::RepeatingClosure callback) = 0;

  // This should follow `CrosSettings::PrepareTrustedValues` logic, but instead
  // of returning Trusted Status, it should return `true` if settings values are
  // trusted and `false` otherwise.
  virtual bool PrepareTrustedValues(base::OnceClosure callback) = 0;

  // Get value in `out_value` and return true if the path is valid, otherwise
  // do not change value and return false.
  virtual bool GetBoolean(const std::string& path, bool* out_value) const = 0;
  virtual bool GetInteger(const std::string& path, int* out_value) const = 0;
  virtual bool GetList(const std::string& path,
                       const base::Value::List** out_value) const = 0;

  // Return whether reporting is enabled or not, allowed settings types will be
  // defined by the implementation.
  virtual bool GetReportingEnabled(const std::string& path,
                                   bool* out_value) const = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_REPORTING_SETTINGS_H_
