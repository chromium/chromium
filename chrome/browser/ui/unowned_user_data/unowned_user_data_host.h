// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_
#define CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_identifier.h"

template <typename T>
class ScopedUnownedUserData;

// This class is a holder for UnownedUserData. There can be only a single entry
// per key, per host. The host must outlive the UnownedUserData. The methods on
// this class should not be used directly, since features should instead be
// retrieved via getters on the individual feature classes.
class UnownedUserDataHost {
 public:
  UnownedUserDataHost();
  ~UnownedUserDataHost();

  template <typename T>
  using Key = ui::TypedIdentifier<T>;
  using UntypedKey = ui::ElementIdentifier;
  template <typename T>
  using PassKey = base::PassKey<ScopedUnownedUserData<T>>;

  // Marks the given `key` as being used in testing. This allows tests to
  // override the value in the map for the given key (which would normally
  // result in a crash).
  template <typename T>
  void MarkKeyForTesting(Key<T> key) {
    MarkKeyForTestingImpl(key.identifier());
  }

  // Sets the entry in the map for the given `key` to `data`.
  // CHECKs that there is no existing entry.
  template <typename T>
  void Set(PassKey<T>, Key<T> key, T& data) {
    SetImpl(key.identifier(), &data);
  }

  // Erases the entry in the map for the given `key`.
  // CHECKs that there *is* an existing entry in the map.
  template <typename T>
  void Erase(PassKey<T>, Key<T> key) {
    EraseImpl(key.identifier());
  }

  // Returns the entry in the map for the given `key`, or null if one does not
  // exist.
  template <typename T>
  T* Get(PassKey<T>, Key<T> key) {
    return static_cast<T*>(GetImpl(key.identifier()));
  }

  template <typename T>
  const T* Get(PassKey<T>, Key<T> key) const {
    return static_cast<const T*>(
        const_cast<UnownedUserDataHost*>(this)->GetImpl(key.identifier()));
  }

 private:
  void MarkKeyForTestingImpl(UntypedKey id);
  void SetImpl(UntypedKey key, void* data);
  void EraseImpl(UntypedKey id);
  void* GetImpl(UntypedKey key);

  std::map<UntypedKey, raw_ptr<void>> map_;
  std::set<UntypedKey> testing_keys_;
};

#endif  // CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_
