// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_PREFS_H_
#define COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_PREFS_H_

class PrefRegistrySimple;

namespace domain_reliability {
namespace prefs {

// Boolean that specifies whether or not domain reliability diagnostic data
// reporting is allowed by policy to be sent over the network.
extern const char kDomainReliabilityAllowedByPolicy[];

}  // namespace prefs

// Registers local state prefs related to Domain Reliability.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_PREFS_H_
