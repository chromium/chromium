// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCOPED_CRITICAL_POLICY_SECTION_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCOPED_CRITICAL_POLICY_SECTION_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Scoped class for ::EnterCriticalPolicySection API. The class make sure we try
// to acquire the section before reading the policy values. It will leave the
// section in the end and self destory.
class POLICY_EXPORT ScopedCriticalPolicySection {
 public:
  struct Handles {
    HANDLE machine_handle;
    HANDLE user_handle;
  };

  using OnSectionEnteredCallback = base::OnceCallback<void(Handles)>;
  using EnterSectionCallback =
      base::OnceCallback<void(OnSectionEnteredCallback)>;

  // Create and own `ScopedCriticalPolicySection` instance. And destory itself
  // after `callback` being invoked.
  // This must be called on the background thread. When loading policy on the
  // main thread, we can't wait for the API as everything must be returned
  // synchronously.
  static void Enter(
      base::OnceClosure callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Same but with custome function to enter critical policy section`. Only
  // used for testing purposes.
  static void EnterWithEnterSectionCallback(
      base::OnceClosure callback,
      EnterSectionCallback enter_section_callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ScopedCriticalPolicySection(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ScopedCriticalPolicySection(const ScopedCriticalPolicySection&) = delete;
  ScopedCriticalPolicySection& operator=(const ScopedCriticalPolicySection&) =
      delete;
  ~ScopedCriticalPolicySection();

  void Init(base::OnceClosure callback);

 private:
  void OnSectionEntered(Handles handles);

  HANDLE machine_handle_ = nullptr;
  HANDLE user_handle_ = nullptr;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OnceClosure callback_;

  EnterSectionCallback enter_section_callback_;

  base::WeakPtrFactory<ScopedCriticalPolicySection> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCOPED_CRITICAL_POLICY_SECTION_H_
