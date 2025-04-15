// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_

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
  kAndroidTabGridDialogShare = 1,
  kAndroidTabGridDialogManage = 2,
  kRecentActivity = 3,
  kAndroidTabGroupContextMenuShare = 4,
  kAndroidTabGroupContextMenuManage = 5,
  kNotification = 6,
  kAndroidMessage = 7,
  kTabGroupItemMenuShare = 8,
  kAndroidShareSheetExtra = 9,
  kDialogToolbarButton = 10,
  kMaxValue = kDialogToolbarButton,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceShareOrManageEntryPoint)

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_FLOW_ENTRY_POINT_H_
