// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_STATE_GENERATOR_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_STATE_GENERATOR_H_

#include <string>

#include "components/url_formatter/spoof_checks/top_domains/trie_entry.h"

namespace url_formatter {

namespace top_domains {

// TopDomainStateGenerator generates C++ code that contains the top domain
// entries in a way the Chromium code understands. The code that reads the
// output can be found in components/url_formatter/idn_spoof_checker.cc.
// The output gets compiled into the binary.
//
// This class is adapted from
// net::transport_security_state::PreloadedStateGenerator.
class TopDomainStateGenerator {
 public:
  TopDomainStateGenerator();
  ~TopDomainStateGenerator();

  // Returns the generated C++ code on success and the empty string on failure.
  std::string Generate(const std::string& template_string,
                       const TopDomainEntries& entries);
};

}  // namespace top_domains

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP_DOMAIN_STATE_GENERATOR_H_
