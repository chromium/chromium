// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that assists preferences in firing notifications when lists
// or dictionaries are changed.

#ifndef COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_
#define COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
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
  ScopedUserPrefUpdateBase(PrefService* service, const std::string& path);

  // Calls Notify().
  ~ScopedUserPrefUpdateBase();

  // Sets |value_| to |service_|->GetMutableUserPref and returns it.
  base::Value* GetValueOfType(base::Value::Type type);

 private:
  // If |value_| is not null, triggers a notification of PrefObservers and
  // resets |value_|.
  void Notify();

  // Weak pointer.
  raw_ptr<PrefService> service_;
  // Path of the preference being updated.
  std::string path_;
  // Cache of value from user pref store (set between Get() and Notify() calls).
  raw_ptr<base::Value> value_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace subtle

// Class to support modifications to dictionary and list base::Values while
// guaranteeing that PrefObservers are notified of changed values.
//
// This class may only be used on the UI thread as it requires access to the
// PrefService.
template <base::Value::Type type_enum_value>
class ScopedUserPrefUpdate : public subtle::ScopedUserPrefUpdateBase {
 public:
  ScopedUserPrefUpdate(PrefService* service, const std::string& path)
      : ScopedUserPrefUpdateBase(service, path) {}

  ScopedUserPrefUpdate(const ScopedUserPrefUpdate&) = delete;
  ScopedUserPrefUpdate& operator=(const ScopedUserPrefUpdate&) = delete;

  // Triggers an update notification if Get() was called.
  virtual ~ScopedUserPrefUpdate() {}

  // Returns a mutable |base::Value| instance that
  // - is already in the user pref store, or
  // - is (silently) created and written to the user pref store if none existed
  //   before.
  //
  // Calling Get() implies that an update notification is necessary at
  // destruction time.
  //
  // The ownership of the return value remains with the user pref store.
  // Virtual so it can be overriden in subclasses that transform the value
  // before returning it (for example to return a subelement of a dictionary).
  virtual base::Value* Get() { return GetValueOfType(type_enum_value); }

  base::Value& operator*() { return *Get(); }

  base::Value* operator->() { return Get(); }
};

typedef ScopedUserPrefUpdate<base::Value::Type::DICTIONARY>
    DictionaryPrefUpdate;
typedef ScopedUserPrefUpdate<base::Value::Type::LIST> ListPrefUpdate;

#endif  // COMPONENTS_PREFS_SCOPED_USER_PREF_UPDATE_H_
