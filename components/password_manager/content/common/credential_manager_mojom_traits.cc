// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_mojom_traits.h"

#include "base/notreached.h"
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
}

// static
password_manager::CredentialType
EnumTraits<blink::mojom::CredentialType, password_manager::CredentialType>::
    FromMojom(blink::mojom::CredentialType input) {
  switch (input) {
    case blink::mojom::CredentialType::EMPTY:
      return password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
    case blink::mojom::CredentialType::PASSWORD:
      return password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD;
    case blink::mojom::CredentialType::FEDERATED:
      return password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
  }

  NOTREACHED();
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
}

// static
password_manager::CredentialManagerError
EnumTraits<blink::mojom::CredentialManagerError,
           password_manager::CredentialManagerError>::
    FromMojom(blink::mojom::CredentialManagerError input) {
  switch (input) {
    case blink::mojom::CredentialManagerError::SUCCESS:
      return password_manager::CredentialManagerError::SUCCESS;
    case blink::mojom::CredentialManagerError::PENDING_REQUEST:
      return password_manager::CredentialManagerError::PENDING_REQUEST;
    case blink::mojom::CredentialManagerError::PASSWORD_STORE_UNAVAILABLE:
      return password_manager::CredentialManagerError::PASSWORDSTOREUNAVAILABLE;
    case blink::mojom::CredentialManagerError::UNKNOWN:
      return password_manager::CredentialManagerError::UNKNOWN;
  }

  NOTREACHED();
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
    case password_manager::CredentialMediationRequirement::kConditional:
      return blink::mojom::CredentialMediationRequirement::kConditional;
  }

  NOTREACHED();
}

// static
password_manager::CredentialMediationRequirement
EnumTraits<blink::mojom::CredentialMediationRequirement,
           password_manager::CredentialMediationRequirement>::
    FromMojom(blink::mojom::CredentialMediationRequirement input) {
  switch (input) {
    case blink::mojom::CredentialMediationRequirement::kSilent:
      return password_manager::CredentialMediationRequirement::kSilent;
    case blink::mojom::CredentialMediationRequirement::kOptional:
      return password_manager::CredentialMediationRequirement::kOptional;
    case blink::mojom::CredentialMediationRequirement::kRequired:
      return password_manager::CredentialMediationRequirement::kRequired;
    case blink::mojom::CredentialMediationRequirement::kConditional:
      return password_manager::CredentialMediationRequirement::kConditional;
  }

  NOTREACHED();
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
