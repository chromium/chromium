// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_DRIVER_H_
#define COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_DRIVER_H_

#include "components/sessions/core/serialized_navigation_driver.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace sessions {

// Provides an iOS implementation of SerializedNavigationDriver that is backed
// by //ios/web classes.
class IOSSerializedNavigationDriver
    : public SerializedNavigationDriver {
 public:
  ~IOSSerializedNavigationDriver() override;

  // Returns the singleton IOSSerializedNavigationDriver.  Almost all
  // callers should use SerializedNavigationDriver::Get() instead.
  static IOSSerializedNavigationDriver* GetInstance();

  // SerializedNavigationDriver implementation.
  int GetDefaultReferrerPolicy() const override;
  std::string GetSanitizedPageStateForPickle(
      const SerializedNavigationEntry* navigation) const override;
  void Sanitize(SerializedNavigationEntry* navigation) const override;
  std::string StripReferrerFromPageState(
      const std::string& page_state) const override;

 private:
  IOSSerializedNavigationDriver();
  friend struct base::DefaultSingletonTraits<IOSSerializedNavigationDriver>;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_DRIVER_H_
