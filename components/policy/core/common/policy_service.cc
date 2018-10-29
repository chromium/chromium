// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service.h"

#include "base/values.h"

namespace policy {

PolicyChangeRegistrar::PolicyChangeRegistrar(PolicyService* policy_service,
                                             const PolicyNamespace& ns)
    : policy_service_(policy_service),
      ns_(ns) {}

PolicyChangeRegistrar::~PolicyChangeRegistrar() {
  if (!callback_map_.empty())
    policy_service_->RemoveObserver(ns_.domain, this);
}

void PolicyChangeRegistrar::Observe(const std::string& policy_name,
                                    const UpdateCallback& callback) {
  if (callback_map_.empty())
    policy_service_->AddObserver(ns_.domain, this);
  callback_map_[policy_name] = callback;
}

void PolicyChangeRegistrar::OnPolicyUpdated(const PolicyNamespace& ns,
                                            const PolicyMap& previous,
                                            const PolicyMap& current) {
  if (ns != ns_)
    return;
  for (auto it = callback_map_.begin(); it != callback_map_.end(); ++it) {
    const base::Value* prev = previous.GetValue(it->first);
    const base::Value* cur = current.GetValue(it->first);

    // Check if the values pointed to by |prev| and |cur| are different.
    if ((!prev ^ !cur) || (prev && cur && *prev != *cur))
      it->second.Run(prev, cur);
  }
}

}  // namespace policy
