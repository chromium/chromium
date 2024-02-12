// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONSTANTS_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONSTANTS_H_

namespace client_certificates {

// Name of the identity representing a managed Profile. This value also
// represents the permanent identity location where new identities will be
// committed when the key pair needs to be rotated.
extern const char kManagedProfileIdentityName[];

// Name of the temporary storage location of an identity during key pair
// rotation.
extern const char kTemporaryManagedProfileIdentityName[];

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONSTANTS_H_
