// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/user_prefs.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/prefs/pref_service.h"

namespace user_prefs {

namespace {

void* UserDataKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int data_key = 0;
  return reinterpret_cast<void*>(&data_key);
}

}  // namespace

// static
PrefService* UserPrefs::Get(base::SupportsUserData* context) {
  DCHECK(context);
  DCHECK(context->GetUserData(UserDataKey()));
  return static_cast<UserPrefs*>(
      context->GetUserData(UserDataKey()))->prefs_;
}

// static
void UserPrefs::Set(base::SupportsUserData* context, PrefService* prefs) {
  DCHECK(context);
  DCHECK(prefs);
  DCHECK(!context->GetUserData(UserDataKey()));
  context->SetUserData(UserDataKey(), base::WrapUnique(new UserPrefs(prefs)));
}

UserPrefs::UserPrefs(PrefService* prefs) : prefs_(prefs) {
}

UserPrefs::~UserPrefs() {
}

}  // namespace user_prefs
