// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
#define COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_

#include "base/macros.h"
#include "build/build_config.h"
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
  ~PrefServiceFlagsStorage() override;

  std::set<std::string> GetFlags() const override;
  bool SetFlags(const std::set<std::string>& flags) override;
  void CommitPendingWrites() override;
  std::string GetOriginListFlag(
      const std::string& internal_entry_name) const override;
  void SetOriginListFlag(const std::string& internal_entry_name,
                         const std::string& origin_list_value) override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

#if defined(OS_CHROMEOS)
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif  // defined(OS_CHROMEOS)

 private:
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceFlagsStorage);
};

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
