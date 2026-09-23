// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/scoped_extra_allowed_domains_for_testing.h"

#include <utility>

#include "components/enterprise/net/core/auth_scope_metadata.h"

namespace enterprise_net {

ScopedExtraAllowedDomainsForTesting::ScopedExtraAllowedDomainsForTesting(
    std::vector<std::string> extra_domains)
    : previous_domains_(GetExtraAllowedDomainsForTesting()) {  // IN-TEST
  SetExtraAllowedDomainsForTesting(std::move(extra_domains));  // IN-TEST
}

ScopedExtraAllowedDomainsForTesting::~ScopedExtraAllowedDomainsForTesting() {
  SetExtraAllowedDomainsForTesting(std::move(previous_domains_));  // IN-TEST
}

}  // namespace enterprise_net
