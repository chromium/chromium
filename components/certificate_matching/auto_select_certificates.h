// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_MATCHING_AUTO_SELECT_CERTIFICATES_H_
#define COMPONENTS_CERTIFICATE_MATCHING_AUTO_SELECT_CERTIFICATES_H_

#include "base/component_export.h"
#include "base/values.h"
#include "net/ssl/client_cert_identity.h"

namespace certificate_matching {

// Partitions `client_certs` according to `auto_selection_filters` (where each
// item corresponds to the "filter" dictionary of an
// AutoSelectCertificateForUrls rule). All certificates that match any filter
// are returned in `matching_client_certs`, and all certificates that don't in
// `nonmatching_client_certs`. Both `matching_client_certs` and
// `nonmatching_client_certs` must be non-null.
COMPONENT_EXPORT(CERTIFICATE_MATCHING)
void AutoSelectCertificates(
    const base::ListValue& auto_selection_filters,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList* matching_client_certs,
    net::ClientCertIdentityList* nonmatching_client_certs);

}  // namespace certificate_matching

#endif  // COMPONENTS_CERTIFICATE_MATCHING_AUTO_SELECT_CERTIFICATES_H_
