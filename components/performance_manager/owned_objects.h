// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_OWNED_OBJECTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_OWNED_OBJECTS_H_

#include <memory>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"

namespace performance_manager {

namespace internal {

// Builds the callback type from the ObjectType and CallbackArgType.
template <typename ObjectType, typename CallbackArgType>
struct CallbackType {
  typedef void (ObjectType::*Type)(CallbackArgType);
};

// Specialization for void CallbackArgType.
template <typename ObjectType>
struct CallbackType<ObjectType, void> {
  typedef void (ObjectType::*Type)();
};

}  // namespace internal

// Helper class defining storage for a collection of "owned" objects. These
// are objects whose ownership has explicitly been passed to the container.
// The objects can be taken back from the container, or will be torn down
// with the container. Note that the owner of this container should
// explicitly call ReleaseObjects prior to the object being torn down; the
// container expects to be empty at destruction.
// TODO: Once C++17 is available, use "auto" here and simply accept the 2
// member function pointers, deducing all other type info.
template <typename OwnedType,
          typename CallbackArgType,
          typename internal::CallbackType<OwnedType, CallbackArgType>::Type
              OnPassedMemberFunction,
          typename internal::CallbackType<OwnedType, CallbackArgType>::Type
              OnTakenMemberFunction>
class OwnedObjects {
 public:
  OwnedObjects() = default;
  ~OwnedObjects() { CHECK(objects_.empty()); }

  OwnedObjects(const OwnedObjects&) = delete;
  OwnedObjects& operator=(const OwnedObjects&) = delete;

  // Passes an object into this container, and invokes the OnPassedFunctionPtr.
  template <typename... ArgTypes>
  void PassObject(std::unique_ptr<OwnedType> object, ArgTypes... args) {
    auto* raw = object.get();
    CHECK(!base::Contains(objects_, raw));
    objects_.insert(std::move(object));
    // We should stop using a flat_set at this point.
    CHECK_GE(100u, objects_.size());
    ((raw)->*(OnPassedMemberFunction))(std::forward<ArgTypes>(args)...);
  }

  // Takes an object back from this container, and invokes the
  // OnTakenFunctionPtr (if the object is found).
  template <typename... ArgTypes>
  std::unique_ptr<OwnedType> TakeObject(OwnedType* raw, ArgTypes... args) {
    std::unique_ptr<OwnedType> object;
    auto it = objects_.find(raw);
    if (it != objects_.end()) {
      CHECK_EQ(raw, it->get());
      // base::flat_set doesn't yet support "extract", but this is the approved
      // way of doing this for now.
      object = std::move(*it);
      objects_.erase(it);
      ((raw)->*(OnTakenMemberFunction))(std::forward<ArgTypes>(args)...);
    }
    return object;
  }

  // Releases all the objects owned by this container, invoking their
  // OnTakenFunctionPtr as they are released.
  template <typename... ArgTypes>
  void ReleaseObjects(ArgTypes... args) {
    // Release the last object first to be friendly with base::flat_set, which
    // is actually a std::vector.
    while (!objects_.empty())
      TakeObject(objects_.rbegin()->get(), std::forward<ArgTypes>(args)...);
  }

  // Returns the current size of this container.
  size_t size() const { return objects_.size(); }

  // Returns true if this container is empty.
  bool empty() const { return objects_.empty(); }

 private:
  // If this ever uses an STL compliant set with "extract", then modify
  // TakeObject to use that instead!
  base::flat_set<std::unique_ptr<OwnedType>, base::UniquePtrComparator>
      objects_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_OWNED_OBJECTS_H_