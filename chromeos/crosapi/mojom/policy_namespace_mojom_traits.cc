// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/policy_namespace_mojom_traits.h"

#include <string_view>

#include "chromeos/crosapi/mojom/policy_domain_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    crosapi::mojom::PolicyNamespaceDataView,
    policy::PolicyNamespace>::Read(crosapi::mojom::PolicyNamespaceDataView data,
                                   policy::PolicyNamespace* out) {
  std::string_view component_id_string;
  if (!data.ReadComponentId(&component_id_string))
    return false;

  policy::PolicyDomain domain;
  switch (data.domain()) {
    case crosapi::mojom::PolicyDomain::kPolicyDomainChrome:
      domain = policy::POLICY_DOMAIN_CHROME;
      break;
    case crosapi::mojom::PolicyDomain::kPolicyDomainExtensions:
      domain = policy::POLICY_DOMAIN_EXTENSIONS;
      break;
    case crosapi::mojom::PolicyDomain::kPolicyDomainSigninExtensions:
      domain = policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS;
      break;
  }

  out->domain = domain;
  out->component_id = std::string{component_id_string};

  return true;
}

}  // namespace mojo
