// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/policy_domain_mojom_traits.h"

namespace mojo {

crosapi::mojom::PolicyDomain
EnumTraits<crosapi::mojom::PolicyDomain, policy::PolicyDomain>::ToMojom(
    policy::PolicyDomain input) {
  switch (input) {
    case policy::POLICY_DOMAIN_CHROME:
      return crosapi::mojom::PolicyDomain::kPolicyDomainChrome;
    case policy::POLICY_DOMAIN_EXTENSIONS:
      return crosapi::mojom::PolicyDomain::kPolicyDomainExtensions;
    case policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      return crosapi::mojom::PolicyDomain::kPolicyDomainSigninExtensions;
    case policy::POLICY_DOMAIN_SIZE:
      NOTREACHED() << "Invalid input " << input;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::PolicyDomain, policy::PolicyDomain>::FromMojom(
    crosapi::mojom::PolicyDomain input,
    policy::PolicyDomain* output) {
  switch (input) {
    case crosapi::mojom::PolicyDomain::kPolicyDomainChrome:
      *output = policy::POLICY_DOMAIN_CHROME;
      return true;
    case crosapi::mojom::PolicyDomain::kPolicyDomainExtensions:
      *output = policy::POLICY_DOMAIN_EXTENSIONS;
      return true;
    case crosapi::mojom::PolicyDomain::kPolicyDomainSigninExtensions:
      *output = policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS;
      return true;
  }

  NOTREACHED();
}

}  // namespace mojo
