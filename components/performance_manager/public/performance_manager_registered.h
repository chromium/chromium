// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_REGISTERED_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_REGISTERED_H_

#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

// This provides functionality that allows an instance of a PM-associated
// object to be looked up by type, allowing the PM to act as a rendezvous
// point for main thread objects. It enforces singleton semantics, so there may
// be no more than one instance of an object of a given type registered with the
// PM at the same time. All registration and unregistration must happen on the
// main thread. It is illegal to register more than one object of the same class
// at a time, and all registered objects must be unregistered prior to PM tear-
// down.

template <typename SelfType>
class PerformanceManagerRegisteredImpl;

// Interface that PerformanceManager registered objects must implement. Should
// only be implemented via PerformanceManagerRegisteredImpl.
class PerformanceManagerRegistered {
 public:
  PerformanceManagerRegistered(const PerformanceManagerRegistered&) = delete;
  PerformanceManagerRegistered& operator=(const PerformanceManagerRegistered&) =
      delete;
  virtual ~PerformanceManagerRegistered() = default;

  // Returns the unique type of the object. Implemented by
  // PerformanceManagerRegisteredImpl.
  virtual uintptr_t GetTypeId() const = 0;

 protected:
  template <typename SelfType>
  friend class PerformanceManagerRegisteredImpl;

  PerformanceManagerRegistered() = default;
};

// Fully implements PerformanceManagerRegistered. Clients should derive from
// this class.
template <typename SelfType>
class PerformanceManagerRegisteredImpl : public PerformanceManagerRegistered {
 public:
  PerformanceManagerRegisteredImpl() = default;
  ~PerformanceManagerRegisteredImpl() override = default;

  // The static TypeId associated with this class.
  static uintptr_t TypeId() {
    // The pointer to this object acts as a unique key that identifies the type
    // at runtime. Note that the address of this should be taken only from a
    // single library, as a different address will be returned from each library
    // into which a given data type is linked. Note that if base/type_id ever
    // becomes a thing, this should use that!
    static constexpr int kTypeId = 0;
    return reinterpret_cast<uintptr_t>(&kTypeId);
  }

  // PerformanceManagerRegistered implementation:
  uintptr_t GetTypeId() const override { return TypeId(); }

  // Helper function for looking up the registered object of this type from the
  // PM. Syntactic sugar for "PerformanceManager::GetRegisteredObjectAs".
  static SelfType* GetFromPM() {
    return PerformanceManager::GetRegisteredObjectAs<SelfType>();
  }

  // Returns true if this object is the registered object in the PM, false
  // otherwise. Useful for DCHECKing contract conditions.
  bool IsRegisteredInPM() const { return GetFromPM() == this; }

  // Returns true if no object of this type is registered in the PM, false
  // otherwise. Useful for DCHECKing contract conditions.
  static bool NothingRegisteredInPM() { return GetFromPM() == nullptr; }
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_REGISTERED_H_