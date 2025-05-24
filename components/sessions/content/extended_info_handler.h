// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_EXTENDED_INFO_HANDLER_H_
#define COMPONENTS_SESSIONS_CONTENT_EXTENDED_INFO_HANDLER_H_

#include <string>

#include "components/sessions/core/sessions_export.h"

namespace content {
class NavigationEntry;
}

namespace sessions {

// This interface is used to store and retrieve arbitrary key/value pairs for
// a NavigationEntry that are not a core part of NavigationEntry.
// WARNING: implementations must deal with versioning. In particular
// RestoreExtendedInfo() may be called with data from a previous version of
// Chrome.
class SESSIONS_EXPORT ExtendedInfoHandler {
 public:
  ExtendedInfoHandler() = default;
  virtual ~ExtendedInfoHandler() = default;

  // Returns the data to write to disk for the specified NavigationEntry.
  virtual std::string GetExtendedInfo(
      content::NavigationEntry* entry) const = 0;

  // Restores |info| which was obtained from a previous call to
  // GetExtendedInfo() to a NavigationEntry.
  virtual void RestoreExtendedInfo(const std::string& info,
                                   content::NavigationEntry* entry) = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_EXTENDED_INFO_HANDLER_H_
