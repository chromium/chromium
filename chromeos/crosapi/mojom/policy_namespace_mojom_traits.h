// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_POLICY_NAMESPACE_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_POLICY_NAMESPACE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/crosapi/mojom/policy_namespace.mojom-shared.h"
#include "components/policy/core/common/policy_namespace.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<crosapi::mojom::PolicyNamespaceDataView,
                 policy::PolicyNamespace> {
  static policy::PolicyDomain domain(const policy::PolicyNamespace& ns) {
    return ns.domain;
  }
  static const std::string& component_id(const policy::PolicyNamespace& ns) {
    return ns.component_id;
  }
  static bool Read(crosapi::mojom::PolicyNamespaceDataView data,
                   policy::PolicyNamespace* out);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_POLICY_NAMESPACE_MOJOM_TRAITS_H_
