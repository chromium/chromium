// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TESTING_PREF_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TESTING_PREF_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "components/prefs/testing_pref_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill::test {

// TBD.
class AutofillTestingPrefService
    : public TestingPrefServiceBase<PrefService,
                                    user_prefs::PrefRegistrySyncable> {
 public:
  AutofillTestingPrefService();

  AutofillTestingPrefService(const AutofillTestingPrefService&) = delete;
  AutofillTestingPrefService& operator=(const AutofillTestingPrefService&) =
      delete;

  ~AutofillTestingPrefService() override;

  // This is provided as a convenience for registering preferences on
  // an existing TestingPrefServiceSyncable instance. On a production
  // PrefService you would do all registrations before constructing
  // it, passing it a PrefRegistry via its constructor (or via
  // e.g. PrefServiceFactory).
  user_prefs::PrefRegistrySyncable* registry();
};

}  // namespace autofill::test

template <>
TestingPrefServiceBase<PrefService, user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(
        scoped_refptr<TestingPrefStore> managed_prefs,
        scoped_refptr<TestingPrefStore> supervised_user_prefs,
        scoped_refptr<TestingPrefStore> extension_prefs,
        scoped_refptr<TestingPrefStore> standalone_browser_prefs,
        scoped_refptr<TestingPrefStore> user_prefs,
        scoped_refptr<TestingPrefStore> recommended_prefs,
        scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
        PrefNotifierImpl* pref_notifier);

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TESTING_PREF_SERVICE_H_
