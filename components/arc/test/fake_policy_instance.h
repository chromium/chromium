// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_

#include <string>

#include "components/arc/mojom/policy.mojom.h"

namespace arc {

class FakePolicyInstance : public mojom::PolicyInstance {
 public:
  FakePolicyInstance();
  ~FakePolicyInstance() override;

  // mojom::PolicyInstance
  void InitDeprecated(mojom::PolicyHostPtr host_ptr) override;
  void Init(mojom::PolicyHostPtr host_ptr, InitCallback callback) override;
  void OnPolicyUpdated() override;
  void OnCommandReceived(const std::string& command,
                         OnCommandReceivedCallback callback) override;

  void CallGetPolicies(mojom::PolicyHost::GetPoliciesCallback callback);

  const std::string& command_payload() { return command_payload_; }

 private:
  mojom::PolicyHostPtr host_ptr_;

  std::string command_payload_;

  DISALLOW_COPY_AND_ASSIGN(FakePolicyInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_POLICY_INSTANCE_H_
