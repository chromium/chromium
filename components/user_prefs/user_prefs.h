// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_USER_PREFS_H_
#define COMPONENTS_USER_PREFS_USER_PREFS_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/user_prefs/user_prefs_export.h"

class PrefService;

namespace user_prefs {

// Components may use preferences associated with a given user. These hang off
// of base::SupportsUserData and can be retrieved using UserPrefs::Get().
//
// It is up to the embedder to create and own the PrefService and attach it to
// base::SupportsUserData using the UserPrefs::Set() function.
class USER_PREFS_EXPORT UserPrefs : public base::SupportsUserData::Data {
 public:
  UserPrefs(const UserPrefs&) = delete;
  UserPrefs& operator=(const UserPrefs&) = delete;

  ~UserPrefs() override;

  // Returns true if there is a PrefService attached to the given context.
  static bool IsInitialized(base::SupportsUserData* context);

  // Retrieves the PrefService for a given context.
  static PrefService* Get(base::SupportsUserData* context);

  // Hangs the specified |prefs| off of |context|. Should be called
  // only once per context.
  static void Set(base::SupportsUserData* context, PrefService* prefs);

 private:
  explicit UserPrefs(PrefService* prefs);

  // Non-owning; owned by embedder.
  raw_ptr<PrefService, AcrossTasksDanglingUntriaged> prefs_;
};

}  // namespace user_prefs

#endif  // COMPONENTS_USER_PREFS_USER_PREFS_H_
