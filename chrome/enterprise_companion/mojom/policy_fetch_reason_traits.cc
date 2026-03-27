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

policy::PolicyFetchReason
EnumTraits<enterprise_companion::mojom::PolicyFetchReason,
           policy::PolicyFetchReason>::
    FromMojom(enterprise_companion::mojom::PolicyFetchReason input) {
  switch (input) {
    case enterprise_companion::mojom::PolicyFetchReason::UNSPECIFIED:
      return policy::PolicyFetchReason::kUnspecified;

    case enterprise_companion::mojom::PolicyFetchReason::USER_REQUEST:
      return policy::PolicyFetchReason::kUserRequest;

    case enterprise_companion::mojom::PolicyFetchReason::SCHEDULED:
      return policy::PolicyFetchReason::kScheduled;
  }

  NOTREACHED();
}

}  // namespace mojo
