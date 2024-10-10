// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

/**
 * Contains information needed to show one row in the activity log UI. This is a shim layer for the
 * native representation of the object. See //components/saved_tab_groups/messaging/activity_log.h
 * for specific details.
 */
public class ActivityLogItem {
    // The type of action associated with the log item.
    @UserAction public int userActionType;

    // Explicit display metadata to be shown in the UI.
    public String titleText;
    public String descriptionText;
    public String timestampText;

    // Implicit metadata that will be used to invoke the delegate when the
    // activity row is clicked.
    public MessageAttribution activityMetadata;
}
