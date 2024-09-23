// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
blink::mojom::CredentialType
EnumTraits<blink::mojom::CredentialType, password_manager::CredentialType>::
    ToMojom(password_manager::CredentialType input) {
  switch (input) {
    case password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY:
      return blink::mojom::CredentialType::EMPTY;
    case password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD:
      return blink::mojom::CredentialType::PASSWORD;
    case password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED:
      return blink::mojom::CredentialType::FEDERATED;
  }

  NOTREACHED_IN_MIGRATION();
  return blink::mojom::CredentialType::EMPTY;
}

// static
bool EnumTraits<blink::mojom::CredentialType,
                password_manager::CredentialType>::
    FromMojom(blink::mojom::CredentialType input,
              password_manager::CredentialType* output) {
  switch (input) {
    case blink::mojom::CredentialType::EMPTY:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
      return true;
    case blink::mojom::CredentialType::PASSWORD:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD;
      return true;
    case blink::mojom::CredentialType::FEDERATED:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::CredentialManagerError
EnumTraits<blink::mojom::CredentialManagerError,
           password_manager::CredentialManagerError>::
    ToMojom(password_manager::CredentialManagerError input) {
  switch (input) {
    case password_manager::CredentialManagerError::SUCCESS:
      return blink::mojom::CredentialManagerError::SUCCESS;
    case password_manager::CredentialManagerError::PENDING_REQUEST:
      return blink::mojom::CredentialManagerError::PENDING_REQUEST;
    case password_manager::CredentialManagerError::PASSWORDSTOREUNAVAILABLE:
      return blink::mojom::CredentialManagerError::PASSWORD_STORE_UNAVAILABLE;
    case password_manager::CredentialManagerError::UNKNOWN:
      return blink::mojom::CredentialManagerError::UNKNOWN;
  }

  NOTREACHED_IN_MIGRATION();
  return blink::mojom::CredentialManagerError::UNKNOWN;
}

// static
bool EnumTraits<blink::mojom::CredentialManagerError,
                password_manager::CredentialManagerError>::
    FromMojom(blink::mojom::CredentialManagerError input,
              password_manager::CredentialManagerError* output) {
  switch (input) {
    case blink::mojom::CredentialManagerError::SUCCESS:
      *output = password_manager::CredentialManagerError::SUCCESS;
      return true;
    case blink::mojom::CredentialManagerError::PENDING_REQUEST:
      *output = password_manager::CredentialManagerError::PENDING_REQUEST;
      return true;
    case blink::mojom::CredentialManagerError::PASSWORD_STORE_UNAVAILABLE:
      *output =
          password_manager::CredentialManagerError::PASSWORDSTOREUNAVAILABLE;
      return true;
    case blink::mojom::CredentialManagerError::UNKNOWN:
      *output = password_manager::CredentialManagerError::UNKNOWN;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::CredentialMediationRequirement
EnumTraits<blink::mojom::CredentialMediationRequirement,
           password_manager::CredentialMediationRequirement>::
    ToMojom(password_manager::CredentialMediationRequirement input) {
  switch (input) {
    case password_manager::CredentialMediationRequirement::kSilent:
      return blink::mojom::CredentialMediationRequirement::kSilent;
    case password_manager::CredentialMediationRequirement::kOptional:
      return blink::mojom::CredentialMediationRequirement::kOptional;
    case password_manager::CredentialMediationRequirement::kRequired:
      return blink::mojom::CredentialMediationRequirement::kRequired;
  }

  NOTREACHED_IN_MIGRATION();
  return blink::mojom::CredentialMediationRequirement::kOptional;
}

// static
bool EnumTraits<blink::mojom::CredentialMediationRequirement,
                password_manager::CredentialMediationRequirement>::
    FromMojom(blink::mojom::CredentialMediationRequirement input,
              password_manager::CredentialMediationRequirement* output) {
  switch (input) {
    case blink::mojom::CredentialMediationRequirement::kSilent:
      *output = password_manager::CredentialMediationRequirement::kSilent;
      return true;
    case blink::mojom::CredentialMediationRequirement::kOptional:
      *output = password_manager::CredentialMediationRequirement::kOptional;
      return true;
    case blink::mojom::CredentialMediationRequirement::kRequired:
      *output = password_manager::CredentialMediationRequirement::kRequired;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<blink::mojom::CredentialInfoDataView,
                  password_manager::CredentialInfo>::
    Read(blink::mojom::CredentialInfoDataView data,
         password_manager::CredentialInfo* out) {
  if (data.ReadType(&out->type) && data.ReadId(&out->id) &&
      data.ReadName(&out->name) && data.ReadIcon(&out->icon) &&
      data.ReadPassword(&out->password) &&
      data.ReadFederation(&out->federation))
    return true;

  return false;
}

}  // namespace mojo
