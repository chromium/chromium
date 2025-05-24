// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_MOJOM_POLICY_FETCH_REASON_TRAITS_H_
#define CHROME_ENTERPRISE_COMPANION_MOJOM_POLICY_FETCH_REASON_TRAITS_H_

#include "chrome/enterprise_companion/mojom/policy_fetch_reason.mojom.h"
#include "components/policy/core/common/policy_types.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<enterprise_companion::mojom::PolicyFetchReason,
                 policy::PolicyFetchReason> {
 public:
  static enterprise_companion::mojom::PolicyFetchReason ToMojom(
      policy::PolicyFetchReason reason);
  static bool FromMojom(enterprise_companion::mojom::PolicyFetchReason input,
                        policy::PolicyFetchReason* output);
};

}  // namespace mojo

#endif  // CHROME_ENTERPRISE_COMPANION_MOJOM_POLICY_FETCH_REASON_TRAITS_H_
