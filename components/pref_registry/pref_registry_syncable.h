// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
#define COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_

#include <stdint.h>

#include <string_view>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"

// TODO(tfarina): Change this namespace to pref_registry.
namespace user_prefs {

// A PrefRegistry for syncable prefs.
//
// Classes or components that want to register such preferences should
// define a static function named RegisterUserPrefs that takes a
// PrefRegistrySyncable*, and the top-level application using the
// class or embedding the component should call this function at an
// appropriate time before the PrefService for these preferences is
// constructed. See e.g. chrome/browser/prefs/browser_prefs.cc which
// does this for Chrome.
//
// TODO(raymes): This class only exists to support SyncableRegistrationCallback
// logic which is only required to support pref registration after the
// PrefService has been created which is only used by tests. We can remove this
// entire class and those tests with some work.
class PrefRegistrySyncable : public PrefRegistrySimple {
 public:
  // Enum of flags used when registering preferences to determine if it should
  // be synced or not. These flags are mutually exclusive, only one of them
  // should ever be specified.
  //
  // Note: These must NOT overlap with PrefRegistry::PrefRegistrationFlags.
  //
  // Note: If adding a new pref with these flags, add the same to the syncable
  // prefs database as well. Refer to components/sync_preferences/README.md for
  // more details about syncable prefs, and chrome/browser/prefs/README.md for
  // details about prefs in general.
  enum PrefRegistrationFlags : uint32_t {
    // The pref will be synced.
    SYNCABLE_PREF = 1 << 0,

    // The pref will be synced. The pref will never be encrypted and will be
    // synced before other datatypes.
    // Because they're never encrypted:
    // -- they can be synced down on first sync before the user is prompted for
    //    a passphrase.
    // -- they are preferred for receiving server-provided data.
    SYNCABLE_PRIORITY_PREF = 1 << 1,

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // As above, but the pref is for an OS settings (e.g. keyboard layout).
    // This distinction allows OS pref sync to be controlled independently from
    // browser pref sync in the UI.
    SYNCABLE_OS_PREF = 1 << 2,
    SYNCABLE_OS_PRIORITY_PREF = 1 << 3,
#endif
  };

  using SyncableRegistrationCallback =
      base::RepeatingCallback<void(std::string_view path, uint32_t flags)>;

  PrefRegistrySyncable();

  PrefRegistrySyncable(const PrefRegistrySyncable&) = delete;
  PrefRegistrySyncable& operator=(const PrefRegistrySyncable&) = delete;

  // Exactly one callback can be set for the event of a syncable
  // preference being registered. It will be fired after the
  // registration has occurred.
  //
  // Calling this method after a callback has already been set will
  // make the object forget the previous callback and use the new one
  // instead.
  void SetSyncableRegistrationCallback(SyncableRegistrationCallback cb);

  // Returns a new PrefRegistrySyncable that uses the same defaults
  // store.
  scoped_refptr<PrefRegistrySyncable> ForkForIncognito();

 private:
  ~PrefRegistrySyncable() override;

  // PrefRegistrySimple overrides.
  void OnPrefRegistered(std::string_view path, uint32_t flags) override;

  SyncableRegistrationCallback callback_;
};

}  // namespace user_prefs

#endif  // COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
