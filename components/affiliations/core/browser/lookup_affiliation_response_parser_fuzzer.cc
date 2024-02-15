// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/affiliations/core/browser/lookup_affiliation_response_parser.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace affiliations {
namespace {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

// We run ParseLookupAffiliationResponse twice with two hardcoded vectors of
// FacetURI. This approach can be extended to generating not only
// LookupAffiliationResponse, but also the vector of FacetURI.
// See more details about the fuzzer extending at
// https://crrev.com/c/1131185/1/components/affiliations/core/browser/lookup_affiliation_response_parser_fuzzer.cc#25
DEFINE_BINARY_PROTO_FUZZER(
    const affiliation_pb::LookupAffiliationByHashPrefixResponse& response) {
  static IcuEnvironment env;

  AffiliationFetcherDelegate::Result result;

  std::vector<FacetURI> uris;
  ParseLookupAffiliationResponse(uris, response, &result);

  uris.push_back(FacetURI::FromCanonicalSpec("https://www.example.com"));
  ParseLookupAffiliationResponse(uris, response, &result);
}

}  // namespace
}  // namespace affiliations
