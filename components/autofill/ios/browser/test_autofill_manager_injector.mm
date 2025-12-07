// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/test_autofill_manager_injector.h"

#import "base/check.h"
#import "base/check_op.h"

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
