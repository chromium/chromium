// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
#define CHROME_BROWSER_UI_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"

class UnownedUserDataHost;

namespace internal {

// Internal base class for handling setting / unsetting the UnownedUserData
// on the UnownedUserDataHost. Should not be used directly outside of this
// file.
class ScopedUnownedUserDataBase {
 protected:
  ScopedUnownedUserDataBase(UnownedUserDataHost& host,
                            const char* key,
                            void* data);
  virtual ~ScopedUnownedUserDataBase();

  static void* GetInternal(UnownedUserDataHost& host, const char* key);

 private:
  using PassKey = base::PassKey<ScopedUnownedUserDataBase>;

  raw_ref<UnownedUserDataHost> host_;
  const char* key_;
  raw_ptr<void> data_;
};

}  // namespace internal

// A scoped class to set and unset an UnownedUserData entry on an
// UnownedUserDataHost.
//
// Example Usage:
//   class MyFeature {
//    public:
//     static const char* kDataKey;
//
//     explicit MyFeature(UnownedUserDataHost& host)
//         : scoped_data_holder_(host) {}
//
//     static MyFeature* FromHost(UnownedUserDataHost& host) {
//       return ScopedUnownedUserData<MyFeature>::Get(host);
//     }
//
//    private:
//     ScopedUnowneduserData<MyFeature> scoped_data_holder_;
//   };
//
//   const char* MyFeature::kDataKey = "MyFeature";
//
// Note: See also UnownedUserDataInterface, if you prefer an inheritance pattern
// and want slightly less boilerplate.
template <class T>
class ScopedUnownedUserData : public internal::ScopedUnownedUserDataBase {
 public:
  ScopedUnownedUserData(UnownedUserDataHost& host, T* data)
      : ScopedUnownedUserDataBase(host, T::kDataKey, data) {}
  ~ScopedUnownedUserData() override = default;

  static T* Get(UnownedUserDataHost& host) {
    return static_cast<T*>(GetInternal(host, T::kDataKey));
  }
};

#endif  // CHROME_BROWSER_UI_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
