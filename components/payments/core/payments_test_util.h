// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENTS_TEST_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENTS_TEST_UTIL_H_

#include <memory>

class PrefService;

namespace payments {

// Common utilities shared amongst Payments tests.
namespace test {

// Return a PrefService that can be used for Payments-related testing in
// contexts where the PrefService would otherwise have to be constructed
// manually (e.g., in unit tests within Autofill core code). The returned
// PrefService has had Autofill preferences registered on its associated
// registry.
std::unique_ptr<PrefService> PrefServiceForTesting();

}  // namespace test
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENTS_TEST_UTIL_H_
