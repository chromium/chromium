// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_SCOPED_EXTRA_ALLOWED_DOMAINS_FOR_TESTING_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_SCOPED_EXTRA_ALLOWED_DOMAINS_FOR_TESTING_H_

#include <string>
#include <vector>

namespace enterprise_net {

// RAII helper to temporarily configure additional destination domains (such as
// localhost or mock test servers) that are permitted to receive OAuth tokens
// during testing.
//
// This helper does not support nesting.
class ScopedExtraAllowedDomainsForTesting {
 public:
  explicit ScopedExtraAllowedDomainsForTesting(
      std::vector<std::string> extra_domains);
  ~ScopedExtraAllowedDomainsForTesting();

  ScopedExtraAllowedDomainsForTesting(
      const ScopedExtraAllowedDomainsForTesting&) = delete;
  ScopedExtraAllowedDomainsForTesting& operator=(
      const ScopedExtraAllowedDomainsForTesting&) = delete;

 private:
  std::vector<std::string> previous_domains_;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_SCOPED_EXTRA_ALLOWED_DOMAINS_FOR_TESTING_H_
