// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_POLICY_DOMAIN_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_POLICY_DOMAIN_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/crosapi/mojom/policy_namespace.mojom-shared.h"
#include "components/policy/core/common/policy_namespace.h"

namespace mojo {

template <>
struct EnumTraits<crosapi::mojom::PolicyDomain, policy::PolicyDomain> {
  static crosapi::mojom::PolicyDomain ToMojom(policy::PolicyDomain input);
  static bool FromMojom(crosapi::mojom::PolicyDomain input,
                        policy::PolicyDomain* output);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_POLICY_DOMAIN_MOJOM_TRAITS_H_
