// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_GOOGLE_CONFIGS_H_
#define COMPONENTS_DOMAIN_RELIABILITY_GOOGLE_CONFIGS_H_

#include <memory>
#include <vector>

#include "components/domain_reliability/config.h"
#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {

// These configs are loaded separately from the "baked-in configs" because they
// are simpler to specify (many parameters are the same) and this avoids parsing
// many boilerplate JSON files. We only construct these on demand to avoid
// needlessly consuming memory for domains that are never visited.

// Returns a Google config that is applicable to the given hostname, or nullptr.
// An exact match is preferred over a superdomain match. An exact match occurs
// if |hostname| is equal to the Google config's hostname, or if |hostname|
// begins with "www." and removing that prefix yields a string equal to a Google
// config's hostname and that config specifies that it should be duplicated for
// the www subdomain. A superdomain match occurs if removing the first label of
// |hostname| yields a string equal to a Google config's hostname and that
// config specifies that it includes subdomains.
std::unique_ptr<const DomainReliabilityConfig> DOMAIN_RELIABILITY_EXPORT
MaybeGetGoogleConfig(const std::string& hostname);

std::vector<std::unique_ptr<const DomainReliabilityConfig>>
    DOMAIN_RELIABILITY_EXPORT GetAllGoogleConfigsForTesting();

}  // namespace domain_reliability

#endif // COMPONENTS_DOMAIN_RELIABILITY_GOOGLE_CONFIGS_H_
