// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_MOJOM_TRAITS_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "components/password_manager/core/common/credential_manager_types.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::CredentialType,
                  password_manager::CredentialType> {
  static blink::mojom::CredentialType ToMojom(
      password_manager::CredentialType input);
  static bool FromMojom(blink::mojom::CredentialType input,
                        password_manager::CredentialType* output);
};

template <>
struct EnumTraits<blink::mojom::CredentialManagerError,
                  password_manager::CredentialManagerError> {
  static blink::mojom::CredentialManagerError ToMojom(
      password_manager::CredentialManagerError input);
  static bool FromMojom(blink::mojom::CredentialManagerError input,
                        password_manager::CredentialManagerError* output);
};

template <>
struct EnumTraits<blink::mojom::CredentialMediationRequirement,
                  password_manager::CredentialMediationRequirement> {
  static blink::mojom::CredentialMediationRequirement ToMojom(
      password_manager::CredentialMediationRequirement input);
  static bool FromMojom(
      blink::mojom::CredentialMediationRequirement input,
      password_manager::CredentialMediationRequirement* output);
};

template <>
struct StructTraits<blink::mojom::CredentialInfoDataView,
                    password_manager::CredentialInfo> {
  static password_manager::CredentialType type(
      const password_manager::CredentialInfo& r) {
    return r.type;
  }

  static const std::optional<std::u16string>& id(
      const password_manager::CredentialInfo& r) {
    return r.id;
  }

  static const std::optional<std::u16string>& name(
      const password_manager::CredentialInfo& r) {
    return r.name;
  }

  static const GURL& icon(const password_manager::CredentialInfo& r) {
    return r.icon;
  }

  static const std::optional<std::u16string>& password(
      const password_manager::CredentialInfo& r) {
    return r.password;
  }

  static const url::SchemeHostPort& federation(
      const password_manager::CredentialInfo& r) {
    return r.federation;
  }

  static bool Read(blink::mojom::CredentialInfoDataView data,
                   password_manager::CredentialInfo* out);
};

}  // namespace mojo

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_MOJOM_TRAITS_H_
