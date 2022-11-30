// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payments_test_util.h"

#include "base/memory/ref_counted.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/payment_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace payments {
namespace test {

std::unique_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  ::payments::RegisterProfilePrefs(registry.get());
  return ::autofill::test::PrefServiceForTesting(registry.get());
}

}  // namespace test
}  // namespace payments
