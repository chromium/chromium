// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pref_registry/pref_registry_syncable.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/default_pref_store.h"

namespace user_prefs {
namespace {

constexpr uint32_t kSyncablePrefFlags =
#if defined(OS_CHROMEOS)
    PrefRegistrySyncable::SYNCABLE_OS_PREF |
    PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF |
#endif
    PrefRegistrySyncable::SYNCABLE_PREF |
    PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF;

}  // namespace

PrefRegistrySyncable::PrefRegistrySyncable() = default;

PrefRegistrySyncable::~PrefRegistrySyncable() = default;

void PrefRegistrySyncable::SetSyncableRegistrationCallback(
    const SyncableRegistrationCallback& cb) {
  callback_ = cb;
}

void PrefRegistrySyncable::OnPrefRegistered(const std::string& path,
                                            uint32_t flags) {
  // Tests that |flags| does not contain both SYNCABLE_PREF and
  // SYNCABLE_PRIORITY_PREF flags at the same time.
  DCHECK(!(flags & PrefRegistrySyncable::SYNCABLE_PREF) ||
         !(flags & PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF));
#if defined(OS_CHROMEOS)
  // Ditto for the mutually exclusive OS pref flags.
  DCHECK(!(flags & PrefRegistrySyncable::SYNCABLE_OS_PREF) ||
         !(flags & PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF));
#endif

  if (flags & kSyncablePrefFlags) {
    if (!callback_.is_null())
      callback_.Run(path, flags);
  }
}

scoped_refptr<PrefRegistrySyncable> PrefRegistrySyncable::ForkForIncognito() {
  // TODO(joi): We can directly reuse the same PrefRegistry once
  // PrefService no longer registers for callbacks on registration and
  // unregistration.
  scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable());
  registry->defaults_ = defaults_;
  registry->registration_flags_ = registration_flags_;
  registry->foreign_pref_keys_ = foreign_pref_keys_;
  return registry;
}

}  // namespace user_prefs
