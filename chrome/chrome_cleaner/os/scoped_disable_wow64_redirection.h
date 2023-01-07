// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SCOPED_DISABLE_WOW64_REDIRECTION_H_
#define CHROME_CHROME_CLEANER_OS_SCOPED_DISABLE_WOW64_REDIRECTION_H_

namespace chrome_cleaner {

// Scoper that switches off Wow64 File System Redirection during its lifetime.
// Taken from: src/components/policy/core/common/policy_loader_win.cc
class ScopedDisableWow64Redirection {
 public:
  ScopedDisableWow64Redirection();

  ScopedDisableWow64Redirection(const ScopedDisableWow64Redirection&) = delete;
  ScopedDisableWow64Redirection& operator=(
      const ScopedDisableWow64Redirection&) = delete;

  ~ScopedDisableWow64Redirection();

  bool is_active() const { return active_; }

 private:
  bool active_;
  void* previous_state_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SCOPED_DISABLE_WOW64_REDIRECTION_H_
