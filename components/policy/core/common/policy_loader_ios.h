// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/policy_export.h"

namespace policy {

class SchemaRegistry;

// A policy loader that loads policy from the managed app configuration
class POLICY_EXPORT PolicyLoaderIOS : public AsyncPolicyLoader {
 public:
  explicit PolicyLoaderIOS(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  PolicyLoaderIOS(const PolicyLoaderIOS&) = delete;
  PolicyLoaderIOS& operator=(const PolicyLoaderIOS&) = delete;
  ~PolicyLoaderIOS() override;

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  PolicyBundle Load() override;

 private:
  // Loads the Chrome policies in |dictionary| into the given |bundle|.
  void LoadNSDictionaryToPolicyBundle(NSDictionary* dictionary,
                                      PolicyBundle* bundle);

  // Validates the given policy data against the stored |schema_|, converting
  // data to the expected type if necessary.  The returned value is suitable for
  // adding to a PolicyMap.
  base::Value ConvertPolicyDataIfNecessary(const std::string& key,
                                           const base::Value& value);

  // The schema used by |ValidatePolicyData()|.
  raw_ptr<const Schema> policy_schema_;

  // Used to Bind() a WeakPtr to |this| for the callback passed to the
  // |notification_observer_|.
  base::WeakPtrFactory<PolicyLoaderIOS> weak_factory_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_H_
