// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_

#include "ui/base/page_transition_types.h"

namespace collaboration {

// Types of entry point to start a join collaboration flow.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceJoinEntryPoint)
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class CollaborationServiceJoinEntryPoint {
  kUnknown = 0,
  kLinkClick = 1,
  kUserTyped = 2,
  kExternalApp = 3,
  kForwardBackButton = 4,
  kRedirect = 5,
  kMaxValue = kRedirect,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceJoinEntryPoint)

// Types of entry point to start a share or manage collaboration flow.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceShareOrManageEntryPoint)
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class CollaborationServiceShareOrManageEntryPoint {
  kUnknown = 0,
  kDialogToolbarButton = 10,
  kRecentActivity = 3,
  kNotification = 6,
  kTabGroupItemMenuShare = 8,

  // Android specific entry points.
  kAndroidTabGridDialogShare = 1,
  kAndroidTabGridDialogManage = 2,
  kAndroidTabGroupContextMenuShare = 4,
  kAndroidTabGroupContextMenuManage = 5,
  kAndroidMessage = 7,
  kAndroidShareSheetExtra = 9,

  // IOS specific entry points.
  kiOSTabGroupIndicatorShare = 11,
  kiOSTabGroupIndicatorManage = 12,
  kiOSTabGridShare = 13,
  kiOSTabGridManage = 14,
  kiOSTabStripShare = 15,
  kiOSTabStripManage = 16,
  kiOSTabGroupViewShare = 17,
  kiOSTabGroupViewManage = 18,
  kiOSMessage = 22,

  // Desktop specific entry points.
  kDesktopGroupEditorShareOrManageButton = 19,
  kDesktopNotification = 20,
  kDesktopRecentActivity = 21,

  kMaxValue = kiOSMessage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceShareOrManageEntryPoint)

// Types of entry point to start a leave or delete collaboration flow.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceLeaveOrDeleteEntryPoint)
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration)
enum class CollaborationServiceLeaveOrDeleteEntryPoint {
  kUnknown = 0,

  // Android specific entry points.
  kAndroidTabGridDialogLeave = 1,
  kAndroidTabGridDialogDelete = 2,
  kAndroidTabGroupContextMenuLeave = 3,
  kAndroidTabGroupContextMenuDelete = 4,
  kAndroidTabGroupItemMenuLeave = 5,
  kAndroidTabGroupItemMenuDelete = 6,
  kAndroidTabGroupRow = 7,
  kMaxValue = kAndroidTabGroupRow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceLeaveOrDeleteEntryPoint)

CollaborationServiceJoinEntryPoint GetEntryPointFromPageTransition(
    ui::PageTransition transition_type);

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_
