// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TYPES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TYPES_H_

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

typedef std::variant<base::Uuid, LocalTabGroupID> EitherGroupID;
typedef std::variant<base::Uuid, LocalTabID> EitherTabID;

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

// Whether the saved tab group is for share or for sync.
enum class SavedTabGroupType {
  // The tab group is synced.
  SYNCED = 0,

  // The tab group is shared.
  SHARED = 1,
};

// LINT.IfChange(OpeningSource)
// Specifies the source of an action that opened a tab group.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class OpeningSource {
  kUnknown = 0,
  // Triggered by opening a tab group from the revisit UI surface.
  kOpenedFromRevisitUi = 1,
  // Android / iOS only. Triggered when a new group added from sync is
  // auto-opened.
  kAutoOpenedFromSync = 2,
  // Desktop only. Triggered by restoring a tab group from the recently closed
  // tabs UI surface. The group could be either of two states: (a) deleted from
  // sync (b) exists in sync, but closed locally.
  kOpenedFromTabRestore = 3,
  // iOS only. Triggered by undo action on close all tabs on the regular tab
  // grid.
  kUndoCloseAllTabs = 4,
  // iOS only. Triggered when user cancels the tab group closure from the dialog
  // shown when the last tab was closed in a group in tab strip.
  kCancelCloseLastTab = 5,
  // Desktop only. Triggered when a group's local ID is reconnected with its
  // sync ID on session restore.
  kConnectOnSessionRestore = 6,
  // Desktop only. Triggered when a unsaved group from v1 implementation is
  // migrated to autosave.
  kAutoSaveOnSessionRestoreForV1Group = 7,

  kMaxValue = kAutoSaveOnSessionRestoreForV1Group,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:GroupOpenReason)

// LINT.IfChange(ClosingSource)
// Specifies the source of an action that closed a tab group.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_group_sync
enum class ClosingSource {
  kUnknown = 0,
  // Android / iOS only. Group was closed by user.
  kClosedByUser = 1,
  // Android / iOS only. Group was explicitly deleted by user.
  kDeletedByUser = 2,
  // Group was deleted from sync.
  kDeletedFromSync = 3,
  // Android / iOS only. Group was closed on startup since it had been already
  // deleted from sync.
  kCleanedUpOnStartup = 4,
  // Android / iOS only. Group was closed when the last chrome instance was
  // closed.
  kCleanedUpOnLastInstanceClosure = 5,
  // Desktop, iOS only. Triggered when user selects close all tabs on the tab
  // grid.
  kCloseAllTabs = 6,
  // iOS only. Triggered when user selects Close Other Tabs.
  kCloseOtherTabs = 7,
  // iOS only. Triggered when user closes the last tab in a group in tab strip.
  kCloseLastTab = 8,
  kMaxValue = kCloseLastTab,
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

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_TYPES_H_
