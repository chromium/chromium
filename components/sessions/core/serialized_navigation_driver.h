// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_DRIVER_H_
#define COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_DRIVER_H_

#include <string>

#include "components/sessions/core/sessions_export.h"

namespace sessions {

class SerializedNavigationEntry;

// The SerializedNavigationDriver interface allows SerializedNavigationEntry to
// obtain information from a singleton driver object. A concrete implementation
// must be provided by the driver on each platform.
class SESSIONS_EXPORT SerializedNavigationDriver {
 public:
  // Returns the singleton SerializedNavigationDriver.
  static SerializedNavigationDriver* Get();

  // Returns the default referrer policy.
  virtual int GetDefaultReferrerPolicy() const = 0;

  // Returns a sanitized version of the given |navigation|'s encoded_page_state
  // suitable for writing to disk.
  virtual std::string GetSanitizedPageStateForPickle(
      const SerializedNavigationEntry* navigation) const = 0;

  // Sanitizes the data in the given |navigation| to be more robust against
  // faulty data written by older versions.
  virtual void Sanitize(SerializedNavigationEntry* navigation) const = 0;

  // Removes the referrer from the encoded page state.
  virtual std::string StripReferrerFromPageState(
      const std::string& page_state) const = 0;

 protected:
  virtual ~SerializedNavigationDriver() = default;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_DRIVER_H_
