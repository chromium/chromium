// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_matching/auto_select_certificates.h"

#include <utility>

#include "base/check.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "net/cert/x509_certificate.h"

namespace certificate_matching {

namespace {

// Returns whether the client certificate matches any of the auto-selection
// filters. Returns false when there's no valid filter.
bool CertMatchesSelectionFilters(
    const net::ClientCertIdentity& client_cert,
    const base::ListValue& auto_selection_filters) {
  for (const auto& filter : auto_selection_filters) {
    if (!filter.is_dict()) {
      // The filter has a wrong format, so ignore it. Note that reporting of
      // schema violations, like this, to UI is already implemented in the
      // policy handler - see configuration_policy_handler_list_factory.cc.
      continue;
    }
    auto issuer_pattern = CertificatePrincipalPattern::ParseFromOptionalDict(
        filter.GetDict().FindDict("ISSUER"), "CN", "L", "O", "OU");
    auto subject_pattern = CertificatePrincipalPattern::ParseFromOptionalDict(
        filter.GetDict().FindDict("SUBJECT"), "CN", "L", "O", "OU");

    if (issuer_pattern.Matches(client_cert.certificate()->issuer()) &&
        subject_pattern.Matches(client_cert.certificate()->subject())) {
      return true;
    }
  }
  return false;
}

}  // namespace

void AutoSelectCertificates(
    const base::ListValue& auto_selection_filters,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList* matching_client_certs,
    net::ClientCertIdentityList* nonmatching_client_certs) {
  CHECK(matching_client_certs);
  CHECK(nonmatching_client_certs);
  matching_client_certs->clear();
  nonmatching_client_certs->clear();
  for (auto& client_cert : client_certs) {
    if (CertMatchesSelectionFilters(*client_cert, auto_selection_filters)) {
      matching_client_certs->push_back(std::move(client_cert));
    } else {
      nonmatching_client_certs->push_back(std::move(client_cert));
    }
  }
}

}  // namespace certificate_matching
