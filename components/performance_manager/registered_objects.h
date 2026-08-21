// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_REGISTERED_OBJECTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_REGISTERED_OBJECTS_H_

#include <type_traits>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"

namespace performance_manager {

// Container for holding registered objects. The objects are stored as raw
// pointers. At most a single instance of an object of a given type may exist
// at a moment. It is expected that RegisteredType satisfies the following
// interface:
//
//   // Returns the type id of the derived type.
//   uintptr_t GetTypeId() const;
//
// The container is expected to be empty by the time of its destruction.
template <typename RegisteredType>
class RegisteredObjects {
 public:
  RegisteredObjects() = default;

  ~RegisteredObjects() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(objects_.empty());
  }

  RegisteredObjects(const RegisteredObjects&) = delete;
  RegisteredObjects& operator=(const RegisteredObjects&) = delete;

  // Registers an object with this container. No more than one object of a given
  // type may be registered at once.
  void RegisterObject(RegisteredType* object) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(object);
    uintptr_t type_id = object->GetTypeId();
    // Find the location to insert the object before, and check if it already
    // contains a `type_id` object.
    auto it = objects_.lower_bound(type_id);
    CHECK(it == objects_.end() || (*it)->GetTypeId() != type_id);
    objects_.insert(it, object);
    // If there are ever so many registered objects we should consider changing
    // data structures.
    CHECK_GT(100u, objects_.size());
  }

  // Unregisters an object from this container. The object must previously have
  // been registered.
  void UnregisterObject(RegisteredType* object) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(object);
    auto it = objects_.find(object->GetTypeId());
    CHECK(it != objects_.end());
    CHECK_EQ(object, *it);
    objects_.erase(it);
  }

  // Returns the object with the registered type, nullptr if none exists.
  RegisteredType* GetRegisteredObject(uintptr_t type_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = objects_.find(type_id);
    if (it == objects_.end())
      return nullptr;
    CHECK_EQ((*it)->GetTypeId(), type_id);
    return *it;
  }

  // Returns the current size of this container.
  size_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return objects_.size();
  }

  // Returns true if this container is empty.
  bool empty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return objects_.empty();
  }

 private:
  // Comparator for registered objects. They are stored by raw pointers but
  // sorted by their type IDs. This is a transparent comparator that also allows
  // comparing with type IDs directly.
  struct RegisteredComparator {
    using is_transparent = void;
    bool operator()(const RegisteredType* r1, const RegisteredType* r2) const {
      return r1->GetTypeId() < r2->GetTypeId();
    }
    bool operator()(const RegisteredType* r1, uintptr_t type_id) const {
      return r1->GetTypeId() < type_id;
    }
    bool operator()(uintptr_t type_id, const RegisteredType* r2) const {
      return type_id < r2->GetTypeId();
    }
  };

  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_set<raw_ptr<RegisteredType, CtnExperimental>, RegisteredComparator>
      objects_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_REGISTERED_OBJECTS_H_