// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_USER_PREFS_H_
#define COMPONENTS_USER_PREFS_USER_PREFS_H_

#include "base/macros.h"
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
  ~UserPrefs() override;

  // Retrieves the PrefService for a given context, or null if none is attached.
  static PrefService* Get(base::SupportsUserData* context);

  // Hangs the specified |prefs| off of |context|. Should be called
  // only once per context.
  static void Set(base::SupportsUserData* context, PrefService* prefs);

 private:
  explicit UserPrefs(PrefService* prefs);

  // Non-owning; owned by embedder.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(UserPrefs);
};

}  // namespace user_prefs

#endif  // COMPONENTS_USER_PREFS_USER_PREFS_H_
