// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_tapjacking_protector_impl.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

AutofillTapjackingProtectorImpl::AutofillTapjackingProtectorImpl(
    AutofillClient* autofill_client)
    : autofill_client_(CHECK_DEREF(autofill_client)) {}

AutofillTapjackingProtectorImpl::~AutofillTapjackingProtectorImpl() = default;

void AutofillTapjackingProtectorImpl::Show(
    AuthorizationType authorization_type,
    base::OnceCallback<void(AuthorizationResult)> callback) {
  // TODO(crbug.com/561395976): Implement tapjacking protection check.
  std::move(callback).Run(AuthorizationResult::kUnknown);
}

}  // namespace autofill
