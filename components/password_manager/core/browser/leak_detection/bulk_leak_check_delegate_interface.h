// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_DELEGATE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_DELEGATE_INTERFACE_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_types.h"

namespace password_manager {

#if !BUILDFLAG(IS_ANDROID)
class LeakCheckCredential;

// Delegate for BulkLeakCheck. Gets the updates during processing the list.
class BulkLeakCheckDelegateInterface {
 public:
  BulkLeakCheckDelegateInterface() = default;
  virtual ~BulkLeakCheckDelegateInterface() = default;

  // Not copyable or movable
  BulkLeakCheckDelegateInterface(const BulkLeakCheckDelegateInterface&) =
      delete;
  BulkLeakCheckDelegateInterface& operator=(
      const BulkLeakCheckDelegateInterface&) = delete;
  BulkLeakCheckDelegateInterface(BulkLeakCheckDelegateInterface&&) = delete;
  BulkLeakCheckDelegateInterface& operator=(BulkLeakCheckDelegateInterface&&) =
      delete;

  // Called when |credential| was processed. |is_leaked| is true if it's leaked.
  virtual void OnFinishedCredential(LeakCheckCredential credential,
                                    IsLeaked is_leaked) = 0;

  // Called when error occurred on one of the credentials. Other credentials are
  // processed further.
  // BulkLeakCheck can be deleted from this call safely.
  virtual void OnError(LeakDetectionError error) = 0;
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_DELEGATE_INTERFACE_H_
