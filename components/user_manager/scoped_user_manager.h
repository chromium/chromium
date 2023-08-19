// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

class UserManager;

namespace internal {

// Implementation details of ScopedUserManager.
class USER_MANAGER_EXPORT ScopedUserManagerImpl {
 public:
  ScopedUserManagerImpl();
  ScopedUserManagerImpl(const ScopedUserManagerImpl&) = delete;
  ScopedUserManagerImpl& operator=(const ScopedUserManagerImpl&) = delete;
  ~ScopedUserManagerImpl();

  void Reset(std::unique_ptr<UserManager> user_manager);
  UserManager* Get() const { return user_manager_.get(); }

 private:
  // Owns the passed UserManager.
  std::unique_ptr<UserManager> user_manager_;

  // Keeps the original UserManager to restore on destruction.
  raw_ptr<UserManager> previous_user_manager_ = nullptr;
};

}  // namespace internal

// Helper class for unit tests. Initializes the UserManager singleton to the
// given UserManager and tears it down again on destruction. If the singleton
// had already been initialized, its previous value is restored after tearing
// down.
//
// Example use case.
//   class FooTest : public testing::Test {
//    public:
//     void SetUp() override {
//       fake_user_manager_.Reset(std::make_unique<FakeUserManager>());
//       fake_user_manager_->AddUser(...);
//       ...
//     }
//    private:
//     TypedScopedUserManager<FakeUserManager> fake_user_manager_;
//   };
//
template <typename T>
class USER_MANAGER_EXPORT TypedScopedUserManager {
 public:
  static_assert(std::is_base_of_v<UserManager, T>,
                "Template parameter of ScopedUserManager must be a subclass "
                "of UserManager.");

  // Do nothing on initialization. Later, Reset() can be called to overwrite
  // global UserManager.
  TypedScopedUserManager() = default;

  // Replaces global UserManager by the given one. On destruction,
  // original one will be restored.
  explicit TypedScopedUserManager(std::unique_ptr<T> user_manager) {
    impl_.Reset(std::move(user_manager));
  }

  // Note: we may want to consider to make this movable, but at this moment
  // there're no use cases found.
  TypedScopedUserManager(const TypedScopedUserManager&) = delete;
  TypedScopedUserManager& operator=(const TypedScopedUserManager&) = delete;

  ~TypedScopedUserManager() { impl_.Reset(nullptr); }

  // Resets the global UserManager by the given one.
  // If the global UserManager is overwritten by another ScopedUserManager,
  // this will fail.
  void Reset(std::unique_ptr<T> user_manager = nullptr) {
    impl_.Reset(std::move(user_manager));
  }

  // Returns the UserManager in specified type.
  T* Get() const { return static_cast<T*>(impl_.Get()); }

  // Operator overloads for better syntax supports:
  T& operator*() const noexcept(noexcept(*std::declval<T*>)) { return *Get(); }
  T* operator->() const noexcept { return Get(); }

 private:
  internal::ScopedUserManagerImpl impl_;
};

using ScopedUserManager = TypedScopedUserManager<UserManager>;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_
