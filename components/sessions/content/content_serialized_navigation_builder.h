// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_

#include <memory>
#include <vector>

#include "components/sessions/core/sessions_export.h"

namespace content {
class BrowserContext;
class NavigationEntry;
class NavigationEntryRestoreContext;
}

namespace sessions {
class SerializedNavigationEntry;

// Provides methods to convert between SerializedNavigationEntry and content
// classes.
class SESSIONS_EXPORT ContentSerializedNavigationBuilder {
 public:
  // Set of options for serializing a navigation. Multiple options can be
  // combined by bit masking.
  enum SerializationOptions {
    // Serialized all available navigation data.
    DEFAULT = 0x0,

    // Exclude page state data. Serializing page state data can involve heavy
    // processing on pages with deep iframe trees, so should be avoided if not
    // necessary.
    EXCLUDE_PAGE_STATE = 0x1,
  };

  // Construct a SerializedNavigationEntry for a particular index from the given
  // NavigationEntry.
  static SerializedNavigationEntry FromNavigationEntry(
      int index,
      content::NavigationEntry* entry,
      SerializationOptions serialization_options =
          SerializationOptions::DEFAULT);

  // Convert the given SerializedNavigationEntry into a NavigationEntry with the
  // given context.  The NavigationEntry will have a transition type of
  // PAGE_TRANSITION_RELOAD and a new unique ID.
  // If a |restore_context| is passed to multiple invocations of this function,
  // it will detect equivalent per-frame state across different
  // SerializedNavigationEntries and de-duplicate the resulting per-frame
  // session history state.
  static std::unique_ptr<content::NavigationEntry> ToNavigationEntry(
      const SerializedNavigationEntry* navigation,
      content::BrowserContext* browser_context,
      content::NavigationEntryRestoreContext* restore_context);

  // Converts a set of SerializedNavigationEntrys into a list of
  // NavigationEntrys with the given context.
  static std::vector<std::unique_ptr<content::NavigationEntry>>
  ToNavigationEntries(const std::vector<SerializedNavigationEntry>& navigations,
                      content::BrowserContext* browser_context);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_
