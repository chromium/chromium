// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
#define COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_

#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/prefs_export.h"

namespace base {
class FilePath;
}

// A simple implementation of PrefRegistry.
class COMPONENTS_PREFS_EXPORT PrefRegistrySimple : public PrefRegistry {
 public:
  PrefRegistrySimple();

  PrefRegistrySimple(const PrefRegistrySimple&) = delete;
  PrefRegistrySimple& operator=(const PrefRegistrySimple&) = delete;

  // For each of these registration methods, |flags| is an optional bitmask of
  // PrefRegistrationFlags.
  void RegisterBooleanPref(std::string_view path,
                           bool default_value,
                           uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterIntegerPref(std::string_view path,
                           int default_value,
                           uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDoublePref(std::string_view path,
                          double default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterStringPref(std::string_view path,
                          std::string_view default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterFilePathPref(std::string_view path,
                            const base::FilePath& default_value,
                            uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterListPref(std::string_view path,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterListPref(std::string_view path,
                        base::Value::List default_value,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDictionaryPref(std::string_view path,
                              uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDictionaryPref(std::string_view path,
                              base::Value::Dict default_value,
                              uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterInt64Pref(std::string_view path,
                         int64_t default_value,
                         uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterUint64Pref(std::string_view path,
                          uint64_t default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterTimePref(std::string_view path,
                        base::Time default_value,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterTimeDeltaPref(std::string_view path,
                             base::TimeDelta default_value,
                             uint32_t flags = NO_REGISTRATION_FLAGS);

 protected:
  ~PrefRegistrySimple() override;
};

#endif  // COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
