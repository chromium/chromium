// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
#define COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_

#include <stdint.h>

#include <memory>
#include <string>

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
  void RegisterBooleanPref(const std::string& path,
                           bool default_value,
                           uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterIntegerPref(const std::string& path,
                           int default_value,
                           uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDoublePref(const std::string& path,
                          double default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterStringPref(const std::string& path,
                          const std::string& default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterFilePathPref(const std::string& path,
                            const base::FilePath& default_value,
                            uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterListPref(const std::string& path,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterListPref(const std::string& path,
                        base::Value::List default_value,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDictionaryPref(const std::string& path,
                              uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterDictionaryPref(const std::string& path,
                              base::Value::Dict default_value,
                              uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterInt64Pref(const std::string& path,
                         int64_t default_value,
                         uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterUint64Pref(const std::string& path,
                          uint64_t default_value,
                          uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterTimePref(const std::string& path,
                        base::Time default_value,
                        uint32_t flags = NO_REGISTRATION_FLAGS);

  void RegisterTimeDeltaPref(const std::string& path,
                             base::TimeDelta default_value,
                             uint32_t flags = NO_REGISTRATION_FLAGS);

 protected:
  ~PrefRegistrySimple() override;
};

#endif  // COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
