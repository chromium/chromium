// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_

#include <map>
#include <ostream>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"

#if BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#endif

namespace policy {

class PolicyBundle;
struct PolicyNamespace;

// A mapping of policy names to PolicyDetails that can be used to set the
// PolicyDetails for test policies.
class PolicyDetailsMap {
 public:
  PolicyDetailsMap();
  PolicyDetailsMap(const PolicyDetailsMap&) = delete;
  PolicyDetailsMap& operator=(const PolicyDetailsMap&) = delete;
  ~PolicyDetailsMap();

  // The returned callback's lifetime is tied to |this| object.
  GetChromePolicyDetailsCallback GetCallback() const;

  // Does not take ownership of |details|.
  void SetDetails(const std::string& policy, const PolicyDetails* details);

 private:
  typedef std::map<std::string, raw_ptr<const PolicyDetails, CtnExperimental>>
      PolicyDetailsMapping;

  const PolicyDetails* Lookup(const std::string& policy) const;

  PolicyDetailsMapping map_;
};

// Returns true if |service| is not serving any policies. Otherwise logs the
// current policies and returns false.
bool PolicyServiceIsEmpty(const PolicyService* service);

#if BUILDFLAG(IS_APPLE)

// Converts a base::Value to the equivalent CFPropertyListRef.
base::apple::ScopedCFTypeRef<CFPropertyListRef> ValueToProperty(
    const base::Value& value);

#endif

std::ostream& operator<<(std::ostream& os, const PolicyBundle& bundle);
std::ostream& operator<<(std::ostream& os, PolicyScope scope);
std::ostream& operator<<(std::ostream& os, PolicyLevel level);
std::ostream& operator<<(std::ostream& os, PolicyDomain domain);
std::ostream& operator<<(std::ostream& os, const PolicyMap& policies);
std::ostream& operator<<(std::ostream& os, const PolicyMap::Entry& e);
std::ostream& operator<<(std::ostream& os, const PolicyNamespace& ns);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_
