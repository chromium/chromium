// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that assists preferences in firing notifications when lists
// or dictionaries are changed.

#ifndef COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_
#define COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/prefs_export.h"

class PrefService;

namespace subtle {

// Base class for ScopedUserPrefUpdateTemplate that contains the parts
// that do not depend on ScopedUserPrefUpdateTemplate's template parameter.
//
// We need this base class mostly for making it a friend of PrefService
// and getting access to PrefService::GetMutableUserPref and
// PrefService::ReportUserPrefChanged.
class COMPONENTS_PREFS_EXPORT ScopedUserPrefUpdateBase {
 public:
  ScopedUserPrefUpdateBase(const ScopedUserPrefUpdateBase&) = delete;
  ScopedUserPrefUpdateBase& operator=(const ScopedUserPrefUpdateBase&) = delete;

 protected:
  ScopedUserPrefUpdateBase(PrefService* service, std::string_view path);

  // Calls Notify().
  virtual ~ScopedUserPrefUpdateBase();

  // Sets `value_` to `service_`->GetMutableUserPref and returns it.
  base::Value* GetValueOfType(base::Value::Type type);

 private:
  // If `value_` is not null, triggers a notification of PrefObservers and
  // resets `value_`.
  void Notify();

  // Weak pointer.
  const raw_ref<PrefService> service_;
  // Path of the preference being updated.
  const std::string path_;
  // Cache of value from user pref store (set between Get() and Notify() calls).
  raw_ptr<base::Value> value_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace subtle

// Class to support modifications to base::Value::Dicts while guaranteeing
// that PrefObservers are notified of changed values.
//
// This class may only be used on the UI thread as it requires access to the
// PrefService.
class COMPONENTS_PREFS_EXPORT ScopedDictPrefUpdate
    : public subtle::ScopedUserPrefUpdateBase {
 public:
  // The underlying dictionary must not be removed from `service` during
  // the lifetime of the created ScopedDictPrefUpdate.
  ScopedDictPrefUpdate(PrefService* service, std::string_view path)
      : ScopedUserPrefUpdateBase(service, path) {}

  ScopedDictPrefUpdate(const ScopedDictPrefUpdate&) = delete;
  ScopedDictPrefUpdate& operator=(const ScopedDictPrefUpdate&) = delete;

  // Triggers an update notification if Get() was called.
  ~ScopedDictPrefUpdate() override = default;

  // Returns a mutable `base::Value::Dict` instance that
  // - is already in the user pref store, or
  // - is (silently) created and written to the user pref store if none existed
  //   before.
  //
  // Calling Get() will result in an update notification automatically
  // being triggered at destruction time.
  //
  // The ownership of the return value remains with the user pref store.
  base::Value::Dict& Get();

  base::Value::Dict& operator*() { return Get(); }

  base::Value::Dict* operator->() { return &Get(); }
};

// Class to support modifications to base::Value::Lists while guaranteeing
// that PrefObservers are notified of changed values.
//
// This class may only be used on the UI thread as it requires access to the
// PrefService.
class COMPONENTS_PREFS_EXPORT ScopedListPrefUpdate
    : public subtle::ScopedUserPrefUpdateBase {
 public:
  // The underlying list must not be removed from `service` during
  // the lifetime of the created ScopedListPrefUpdate.
  ScopedListPrefUpdate(PrefService* service, std::string_view path)
      : ScopedUserPrefUpdateBase(service, path) {}

  ScopedListPrefUpdate(const ScopedListPrefUpdate&) = delete;
  ScopedListPrefUpdate& operator=(const ScopedListPrefUpdate&) = delete;

  // Triggers an update notification if Get() was called.
  ~ScopedListPrefUpdate() override = default;

  // Returns a mutable `base::Value::List` instance that
  // - is already in the user pref store, or
  // - is (silently) created and written to the user pref store if none existed
  //   before.
  //
  // Calling Get() will result in an update notification automatically
  // being triggered at destruction time.
  //
  // The ownership of the return value remains with the user pref store.
  base::Value::List& Get();

  base::Value::List& operator*() { return Get(); }

  base::Value::List* operator->() { return &Get(); }
};

#endif  // COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_
