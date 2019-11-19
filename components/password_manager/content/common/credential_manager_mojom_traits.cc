// Copyright 2016 The Chromium Authors. All rights reserved.
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

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
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
    case blink::mojom::CredentialManagerError::NOT_ALLOWED:
    case blink::mojom::CredentialManagerError::ANDROID_NOT_SUPPORTED_ERROR:
    case blink::mojom::CredentialManagerError::ANDROID_ALGORITHM_UNSUPPORTED:
    case blink::mojom::CredentialManagerError::ANDROID_EMPTY_ALLOW_CREDENTIALS:
    case blink::mojom::CredentialManagerError::
        ANDROID_USER_VERIFICATION_UNSUPPORTED:
    case blink::mojom::CredentialManagerError::INVALID_DOMAIN:
    case blink::mojom::CredentialManagerError::INVALID_ICON_URL:
    case blink::mojom::CredentialManagerError::CREDENTIAL_EXCLUDED:
    case blink::mojom::CredentialManagerError::CREDENTIAL_NOT_RECOGNIZED:
    case blink::mojom::CredentialManagerError::NOT_IMPLEMENTED:
    case blink::mojom::CredentialManagerError::NOT_FOCUSED:
    case blink::mojom::CredentialManagerError::RESIDENT_CREDENTIALS_UNSUPPORTED:
    case blink::mojom::CredentialManagerError::PROTECTION_POLICY_INCONSISTENT:
    case blink::mojom::CredentialManagerError::ABORT:
    case blink::mojom::CredentialManagerError::OPAQUE_DOMAIN:
    case blink::mojom::CredentialManagerError::INVALID_PROTOCOL:
    case blink::mojom::CredentialManagerError::BAD_RELYING_PARTY_ID:
    case blink::mojom::CredentialManagerError::UNKNOWN:
      *output = password_manager::CredentialManagerError::UNKNOWN;
      return true;
  }

  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
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
