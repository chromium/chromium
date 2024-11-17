// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

/**
 * Contains information needed to show one row in the activity log UI. This is a shim layer for the
 * native representation of the object. See
 * //components/collaboration/public/messaging/activity_log.h for specific details.
 */
public class ActivityLogItem {
    // The type of collaboration event associated with the log item.
    @CollaborationEvent public int collaborationEvent;

    // Explicit display metadata to be shown in the UI.
    public String titleText;
    public String descriptionText;
    public String timestampText;

    // Implicit metadata that will be used to invoke the delegate when the
    // activity row is clicked.
    public MessageAttribution activityMetadata;
}
