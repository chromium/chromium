// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/test_autofill_manager_injector.h"

#include "base/check.h"
#include "base/check_op.h"

namespace autofill {

size_t TestAutofillManagerInjectorBase::num_instances_ = 0;

TestAutofillManagerInjectorBase::TestAutofillManagerInjectorBase() {
  CHECK(!some_instance_is_alive())
      << "At most one instance is allowed per TestAutofillManagerInjector";
  ++num_instances_;
}

TestAutofillManagerInjectorBase::~TestAutofillManagerInjectorBase() {
  DCHECK_GE(num_instances_, 1u);
  --num_instances_;
}

}  // namespace autofill
