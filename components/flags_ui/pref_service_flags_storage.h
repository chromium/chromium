// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
#define COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/flags_storage.h"

class PrefService;
class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace flags_ui {

// Implements the FlagsStorage interface with a PrefService backend.
// This is the implementation used on desktop Chrome for all users.
class PrefServiceFlagsStorage : public FlagsStorage {
 public:
  explicit PrefServiceFlagsStorage(PrefService* prefs);

  PrefServiceFlagsStorage(const PrefServiceFlagsStorage&) = delete;
  PrefServiceFlagsStorage& operator=(const PrefServiceFlagsStorage&) = delete;

  ~PrefServiceFlagsStorage() override;

  std::set<std::string> GetFlags() const override;
  bool SetFlags(const std::set<std::string>& flags) override;
  void CommitPendingWrites() override;
  std::string GetOriginListFlag(
      const std::string& internal_entry_name) const override;
  void SetOriginListFlag(const std::string& internal_entry_name,
                         const std::string& origin_list_value) override;
  std::string GetStringFlag(
      const std::string& internal_entry_name) const override;
  void SetStringFlag(const std::string& internal_entry_name,
                     const std::string& string_value) override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  raw_ptr<PrefService, DanglingUntriaged> prefs_;
};

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
