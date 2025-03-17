// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_REGISTRY_H_
#define COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_REGISTRY_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "url/gurl.h"

namespace ip_protection {

// Class ProbabilisticRevealTokenRegistry is a pseudo-singleton owned by the
// NetworkService. It parses the json content from the component updater.
class ProbabilisticRevealTokenRegistry {
 public:
  ProbabilisticRevealTokenRegistry();
  ~ProbabilisticRevealTokenRegistry();
  ProbabilisticRevealTokenRegistry(const ProbabilisticRevealTokenRegistry&);
  ProbabilisticRevealTokenRegistry& operator=(
      const ProbabilisticRevealTokenRegistry&);

  // Determines if the request is eligible to receive a token header.
  bool IsRegistered(const GURL& request_url);

  // Clears the existing registry and replaces it with the new content.
  void UpdateRegistry(base::Value::Dict registry);

 private:
  base::flat_set<std::string> domains_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_REGISTRY_H_
