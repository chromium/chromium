// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_CORE_LIVE_TAB_H_

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_service.h"

namespace sessions {

// Interface that represents a currently-live tab to the core sessions code, and
// in particular, the tab restore service. This interface abstracts the concrete
// representation of a live tab on different platforms (e.g., WebContents on
// //content-based platforms).
class SESSIONS_EXPORT LiveTab {
 public:
  virtual ~LiveTab();

  // Methods that return information about the navigation state of the tab.
  virtual bool IsInitialBlankNavigation() = 0;
  virtual int GetCurrentEntryIndex() = 0;
  virtual int GetPendingEntryIndex() = 0;
  virtual SerializedNavigationEntry GetEntryAtIndex(int index) = 0;
  virtual SerializedNavigationEntry GetPendingEntry() = 0;
  virtual int GetEntryCount() = 0;

  // Returns any platform-specific data that should be associated with the
  // tab_restore::Tab corresponding to this instance. The default
  // implementation returns null.
  virtual std::unique_ptr<tab_restore::PlatformSpecificTabData>
  GetPlatformSpecificTabData();

  // Returns the user agent override, if any.
  virtual SerializedUserAgentOverride GetUserAgentOverride() = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_LIVE_TAB_H_
