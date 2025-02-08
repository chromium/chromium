// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/mojom/policy_fetch_reason_traits.h"

#include "base/notreached.h"
#include "chrome/enterprise_companion/mojom/policy_fetch_reason.mojom.h"
#include "components/policy/core/common/policy_types.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

enterprise_companion::mojom::PolicyFetchReason EnumTraits<
    enterprise_companion::mojom::PolicyFetchReason,
    policy::PolicyFetchReason>::ToMojom(policy::PolicyFetchReason input) {
  switch (input) {
    case policy::PolicyFetchReason::kUnspecified:
      return enterprise_companion::mojom::PolicyFetchReason::UNSPECIFIED;

    case policy::PolicyFetchReason::kUserRequest:
      return enterprise_companion::mojom::PolicyFetchReason::USER_REQUEST;

    case policy::PolicyFetchReason::kScheduled:
      return enterprise_companion::mojom::PolicyFetchReason::SCHEDULED;

    default:
      NOTREACHED();
  }
}

bool EnumTraits<enterprise_companion::mojom::PolicyFetchReason,
                policy::PolicyFetchReason>::
    FromMojom(enterprise_companion::mojom::PolicyFetchReason input,
              policy::PolicyFetchReason* output) {
  switch (input) {
    case enterprise_companion::mojom::PolicyFetchReason::UNSPECIFIED:
      *output = policy::PolicyFetchReason::kUnspecified;
      return true;

    case enterprise_companion::mojom::PolicyFetchReason::USER_REQUEST:
      *output = policy::PolicyFetchReason::kUserRequest;
      return true;

    case enterprise_companion::mojom::PolicyFetchReason::SCHEDULED:
      *output = policy::PolicyFetchReason::kScheduled;
      return true;
  }

  return false;
}

}  // namespace mojo
