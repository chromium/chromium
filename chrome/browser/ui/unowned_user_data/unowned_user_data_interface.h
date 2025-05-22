// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_INTERFACE_H_
#define CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_INTERFACE_H_

#include "chrome/browser/ui/unowned_user_data/scoped_unowned_user_data.h"

class UnownedUserDataHost;

// An interface to handle setting / unsetting an UnownedUserData entry on an
// UnownedUserDataHost. This interface provides the Get() method and ownership
// of the ScopedUnownedUserData<> member to reduce the boilerplate of
// subclasses.
// Since this is not a pure virtual class, consumers may also use
// ScopedUnownedUserData<> directly to avoid multiple inheritance.
template <class T>
class UnownedUserDataInterface {
 public:
  UnownedUserDataInterface(UnownedUserDataHost& host, T* data)
      : scoped_user_data_(host, data) {}
  virtual ~UnownedUserDataInterface() = default;

  static T* Get(UnownedUserDataHost& host) {
    return ScopedUnownedUserData<T>::Get(host);
  }

 private:
  ScopedUnownedUserData<T> scoped_user_data_;
};

#endif  // CHROME_BROWSER_UI_UNOWNED_USER_DATA_UNOWNED_USER_DATA_INTERFACE_H_
