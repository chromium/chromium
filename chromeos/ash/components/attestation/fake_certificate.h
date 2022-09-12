// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_CERTIFICATE_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_CERTIFICATE_H_

#include <string>

#include "base/time/time.h"

namespace ash {
namespace attestation {

// Creates a self-signed |certificate| based on constant key material.  The
// certificate |expiry| is relative to the current time.  The certificate will
// be (or have been) valid from sometime before the current time or expiry,
// whichever is first.  This is designed for use in unit tests and runs quickly.
// Returns true on success.
bool GetFakeCertificateDER(const base::TimeDelta& expiry,
                           std::string* certificate);

// Similar to GetFakeCertificateDER but returns a PEM-encoded certificate.
bool GetFakeCertificatePEM(const base::TimeDelta& expiry,
                           std::string* certificate);

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_CERTIFICATE_H_
