// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_
#define CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_

#include <map>

#include "base/types/pass_key.h"

namespace internal {
class ScopedUnownedUserDataBase;
}

// This class is a holder for UnownedUserData. There can be only a single entry
// per key, per host. The host must outlive the UnownedUserData. The methods on
// this class should not be used directly, since features should instead be
// retrieved via getters on the individual feature classes.
class UnownedUserDataHost {
 public:
  UnownedUserDataHost();
  ~UnownedUserDataHost();

  // Sets the entry in the map for the given `key` to `data`.
  // CHECKs that there is no existing entry.
  void Set(base::PassKey<internal::ScopedUnownedUserDataBase> pass_key,
           const char* key,
           void* data);

  // Erases the entry in the map for the given `key`.
  // CHECKs that there *is* an existing entry in the map.
  void Erase(base::PassKey<internal::ScopedUnownedUserDataBase> pass_key,
             const char* key);

  // Returns the entry in the map for the given `key`, or null if one does not
  // exist.
  void* Get(base::PassKey<internal::ScopedUnownedUserDataBase>,
            const char* key);

 private:
  std::map<const char*, void*> map_;
};

#endif  // CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_HOST_H_
