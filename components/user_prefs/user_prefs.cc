// Copyright 2013 The Chromium Authors
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
bool UserPrefs::IsInitialized(base::SupportsUserData* context) {
  CHECK(context);
  return context->GetUserData(UserDataKey()) != nullptr;
}

// static
bool UserPrefs::ArePrefsLoaded(base::SupportsUserData* context) {
  if (!context || !IsInitialized(context)) {
    return false;
  }
  PrefService::PrefInitializationStatus status =
      Get(context)->GetInitializationStatus();
  // status 1 = SUCCESS, status 2 = CREATED_NEW_PREF_STORE. Both mean the store
  // is loaded and ready to use.
  return status == PrefService::INITIALIZATION_STATUS_SUCCESS ||
         status == PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

// static
PrefService* UserPrefs::Get(base::SupportsUserData* context) {
  DCHECK(context);
  DCHECK(IsInitialized(context));
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
