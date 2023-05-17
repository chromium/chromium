// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_registry_simple.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"

PrefRegistrySimple::PrefRegistrySimple() = default;
PrefRegistrySimple::~PrefRegistrySimple() = default;

void PrefRegistrySimple::RegisterBooleanPref(const std::string& path,
                                             bool default_value,
                                             uint32_t flags) {
  RegisterPreference(path, base::Value(default_value), flags);
}

void PrefRegistrySimple::RegisterIntegerPref(const std::string& path,
                                             int default_value,
                                             uint32_t flags) {
  RegisterPreference(path, base::Value(default_value), flags);
}

void PrefRegistrySimple::RegisterDoublePref(const std::string& path,
                                            double default_value,
                                            uint32_t flags) {
  RegisterPreference(path, base::Value(default_value), flags);
}

void PrefRegistrySimple::RegisterStringPref(const std::string& path,
                                            const std::string& default_value,
                                            uint32_t flags) {
  RegisterPreference(path, base::Value(default_value), flags);
}

void PrefRegistrySimple::RegisterFilePathPref(
    const std::string& path,
    const base::FilePath& default_value,
    uint32_t flags) {
  RegisterPreference(path, base::Value(default_value.AsUTF8Unsafe()), flags);
}

void PrefRegistrySimple::RegisterListPref(const std::string& path,
                                          uint32_t flags) {
  RegisterPreference(path, base::Value(base::Value::Type::LIST), flags);
}

void PrefRegistrySimple::RegisterListPref(const std::string& path,
                                          base::Value::List default_value,
                                          uint32_t flags) {
  RegisterPreference(path, base::Value(std::move(default_value)), flags);
}

void PrefRegistrySimple::RegisterDictionaryPref(const std::string& path,
                                                uint32_t flags) {
  RegisterPreference(path, base::Value(base::Value::Type::DICT), flags);
}

void PrefRegistrySimple::RegisterDictionaryPref(const std::string& path,
                                                base::Value::Dict default_value,
                                                uint32_t flags) {
  RegisterPreference(path, base::Value(std::move(default_value)), flags);
}

void PrefRegistrySimple::RegisterInt64Pref(const std::string& path,
                                           int64_t default_value,
                                           uint32_t flags) {
  RegisterPreference(path, base::Value(base::NumberToString(default_value)),
                     flags);
}

void PrefRegistrySimple::RegisterUint64Pref(const std::string& path,
                                            uint64_t default_value,
                                            uint32_t flags) {
  RegisterPreference(path, base::Value(base::NumberToString(default_value)),
                     flags);
}

void PrefRegistrySimple::RegisterTimePref(const std::string& path,
                                          base::Time default_value,
                                          uint32_t flags) {
  RegisterInt64Pref(
      path, default_value.ToDeltaSinceWindowsEpoch().InMicroseconds(), flags);
}

void PrefRegistrySimple::RegisterTimeDeltaPref(const std::string& path,
                                               base::TimeDelta default_value,
                                               uint32_t flags) {
  RegisterInt64Pref(path, default_value.InMicroseconds(), flags);
}
