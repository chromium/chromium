// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/test_autofill_driver_injector.h"

#include "base/check.h"
#include "base/check_op.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"

namespace autofill {

size_t TestAutofillDriverInjectorBase::num_instances_ = 0;

TestAutofillDriverInjectorBase::TestAutofillDriverInjectorBase() {
  CHECK(!some_instance_is_alive())
      << "At most one instance is allowed per TestAutofillDriverInjectors";
  CHECK(!TestAutofillManagerInjectorBase::some_instance_is_alive())
      << "TestAutofillDriverInjector must be created before any "
         "TestAutofillManagerInjector";
  ++num_instances_;
}

TestAutofillDriverInjectorBase::~TestAutofillDriverInjectorBase() {
  DCHECK_GE(num_instances_, 1u);
  --num_instances_;
}

}  // namespace autofill
