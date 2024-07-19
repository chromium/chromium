// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_

#include <optional>

#include "base/logging.h"
#include "base/token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups {

// Types for tab and tab group IDs.
#if BUILDFLAG(IS_ANDROID)
using LocalTabID = int;
using LocalTabGroupID = base::Token;
#elif BUILDFLAG(IS_IOS)
using LocalTabID = int;
using LocalTabGroupID = tab_groups::TabGroupId;
#else
using LocalTabID = base::Token;
using LocalTabGroupID = tab_groups::TabGroupId;
#endif

// Base context for tab group actions. Platforms can subclass this to pass
// additional context such as a browser window.
struct TabGroupActionContext {
  virtual ~TabGroupActionContext();
};

// Whether the update was originated by a change in the local or remote
// client.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class TriggerSource {
  // The source is a remote chrome client.
  REMOTE = 0,

  // The source is the local chrome client.
  LOCAL = 1,
};

// LINT.IfChange(OpeningSource)
// Specifies the source of an action that opened a tab group.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class OpeningSource {
  kUnknown = 0,
  kOpenedFromRevisitUi = 1,
  kAutoOpenedFromSync = 2,
  kOpenedFromTabRestore = 3,
  kMaxValue = kOpenedFromTabRestore,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:GroupOpenReason)

// LINT.IfChange(ClosingSource)
// Specifies the source of an action that closed a tab group.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class ClosingSource {
  kUnknown = 0,
  kClosedByUser = 1,
  kDeletedByUser = 2,
  kDeletedFromSync = 3,
  kCleanedUpOnStartup = 4,
  kCleanedUpOnLastInstanceClosure = 5,
  kMaxValue = kCleanedUpOnLastInstanceClosure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:GroupCloseReason)

// LINT.IfChange(TabGroupEvent)
// Various types of mutation events associated with tab groups and tabs.
// Used for metrics only. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class TabGroupEvent {
  kTabGroupCreated = 0,
  kTabGroupRemoved = 1,
  kTabGroupOpened = 2,
  kTabGroupClosed = 3,
  kTabGroupVisualsChanged = 4,
  kTabGroupTabsReordered = 5,
  kTabAdded = 6,
  kTabRemoved = 7,
  kTabNavigated = 8,
  kTabSelected = 9,
  kMaxValue = kTabSelected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:TabGroupEvent)

// Helper struct to pass around information about tab group
// events for recording metrics.
struct EventDetails {
  TabGroupEvent event_type;
  std::optional<LocalTabGroupID> local_tab_group_id;
  std::optional<LocalTabID> local_tab_id;
  std::optional<OpeningSource> opening_source;
  std::optional<ClosingSource> closing_source;

  explicit EventDetails(TabGroupEvent event_type);
  ~EventDetails();
  EventDetails(const EventDetails& other);
  EventDetails& operator=(const EventDetails& other);
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_
