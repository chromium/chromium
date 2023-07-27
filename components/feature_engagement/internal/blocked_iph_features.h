// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_BLOCKED_IPH_FEATURES_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_BLOCKED_IPH_FEATURES_H_

#include <map>
#include <string>

#include "base/synchronization/lock.h"

namespace base {
class CommandLine;
template <typename T>
class NoDestructor;
}  // namespace base

namespace feature_engagement {

namespace test {
class ScopedIphFeatureList;
}

// Stores information about whether IPH is globally blocked by default, and if
// so, what if any IPH are being allowed. This is primarily used by tests via
// `ScopedIphFeatureList` (of which both browser_tests and interactive_ui_tests
// automatically create one to suppress IPH) but can be required if a test
// launches a vanilla chrome process that also must suppress or only allow
// certain IPH.
//
// Whether a feature is blocked or not is independent of whether the feature
// flag is actually enabled.
//
// This object is a per-process singleton; get with `GetInstance()`.
//
// This object is thread-safe. Because this object can be accessed during test
// setup and on the browser main thread and test threads (and because it could
// be lazily created on any thread), it uses a lock rather than a sequence
// checker. Accesses should be short and fairly inconsequential, so there is
// little harm in the extra security.
class BlockedIphFeatures {
 public:
  ~BlockedIphFeatures();
  BlockedIphFeatures(const BlockedIphFeatures&) = delete;
  void operator=(const BlockedIphFeatures&) = delete;

  static BlockedIphFeatures* GetInstance();

  // Returns whether the given feature is blocked in the current scope.
  bool IsFeatureBlocked(const std::string& feature_name) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Used to propagate state to child processes. Writes both a special flag that
  // conveys what type of IPH are allowed, and also potentially adds/adds to the
  // --enable-features flag to turn those IPH on.
  void MaybeWriteToCommandLine(base::CommandLine& command_line) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::Lock& GetLock() const LOCK_RETURNED(lock_) { return lock_; }

 private:
  friend test::ScopedIphFeatureList;
  friend class base::NoDestructor<BlockedIphFeatures>;
  friend class BlockedIphFeaturesTest;

  BlockedIphFeatures();

  // The following API is used only by `test::ScopedIphFeatureList` and
  // internally by this class. This way, reference counts are kept consistent.

  // Increments or decrements the global number of entities which have requested
  // global blocking of IPH by default.
  void IncrementGlobalBlockCount() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecrementGlobalBlockCount() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Increments or decrements the number of entities which have requested
  // that the feature with `feature_name` be allow-listed through a global
  // block.
  void IncrementFeatureAllowedCount(const std::string& feature_name)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecrementFeatureAllowedCount(const std::string& feature_name)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void MaybeReadFromCommandLine() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  static const char kPropagateIPHForTestingSwitch[];

  size_t global_block_count_ = 0;
  std::map<std::string, size_t> allow_feature_counts_;
  bool read_from_command_line_ = false;
  mutable base::Lock lock_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_BLOCKED_IPH_FEATURES_H_
