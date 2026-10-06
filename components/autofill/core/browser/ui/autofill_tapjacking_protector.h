// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_TAPJACKING_PROTECTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_TAPJACKING_PROTECTOR_H_

#include "base/functional/callback_forward.h"

namespace autofill {

// Abstract base class providing tapjacking protection for sensitive Autofill
// actions (such as payments authorization). Triggers device authentication if
// available, falls back to a confirmation dialog otherwise.
class AutofillTapjackingProtector {
 public:
  enum class AuthorizationType {
    kPayments,
  };

  enum class AuthorizationResult {
    kSuccess,
    kCancelled,
    kUnknown,
  };

  using AuthorizationCallback = base::OnceCallback<void(AuthorizationResult)>;

  AutofillTapjackingProtector();

  AutofillTapjackingProtector(const AutofillTapjackingProtector&) = delete;
  AutofillTapjackingProtector& operator=(const AutofillTapjackingProtector&) =
      delete;
  virtual ~AutofillTapjackingProtector();

  virtual void Show(AuthorizationType authorization_type,
                    AuthorizationCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_TAPJACKING_PROTECTOR_H_
