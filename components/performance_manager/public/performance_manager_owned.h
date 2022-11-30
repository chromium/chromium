// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OWNED_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OWNED_H_

namespace performance_manager {

// Helper class for passing ownership of objects to the PerformanceManager.
// The object is expected to live on the main thread.
class PerformanceManagerOwned {
 public:
  virtual ~PerformanceManagerOwned() = default;

  PerformanceManagerOwned(const PerformanceManagerOwned&) = delete;
  PerformanceManagerOwned& operator=(const PerformanceManagerOwned&) = delete;

  // Called when the object is passed into the PerformanceManager.
  virtual void OnPassedToPM() = 0;

  // Called when the object is removed from the PerformanceManager, either via
  // an explicit call to TakeFromPM, or prior to the PerformanceManager being
  // destroyed.
  virtual void OnTakenFromPM() = 0;

 protected:
  PerformanceManagerOwned() = default;
};

// A default implementation of PerformanceManagerOwned.
class PerformanceManagerOwnedDefaultImpl : public PerformanceManagerOwned {
 public:
  ~PerformanceManagerOwnedDefaultImpl() override = default;

  PerformanceManagerOwnedDefaultImpl(
      const PerformanceManagerOwnedDefaultImpl&) = delete;
  PerformanceManagerOwnedDefaultImpl& operator=(
      const PerformanceManagerOwnedDefaultImpl*) = delete;

  // PerformanceManagerOwned implementation:
  void OnPassedToPM() override {}
  void OnTakenFromPM() override {}

 protected:
  PerformanceManagerOwnedDefaultImpl() = default;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OWNED_H_
