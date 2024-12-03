// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import androidx.annotation.Nullable;

/**
 * Contains information needed to show one row in the activity log UI. This is a shim layer for the
 * native representation of the object. See
 * //components/collaboration/public/messaging/activity_log.h for specific details.
 */
public class ActivityLogItem {
    // The type of collaboration event associated with the log item.
    public @CollaborationEvent int collaborationEvent;

    // Display name of the user.
    public String userDisplayName;

    // Whether the user associated with the activity log item is the current signed in user
    // themselves.
    public boolean userIsSelf;

    // Description text to be shown on first half of the description line. Timestamp will be
    // appended.
    public @Nullable String description;

    // The time duration in milliseconds that has passed since the action happened. Used for
    // generating the relative duration text that will be appended to the description. If the
    // description is empty, the entire description line will contain only the relative duration
    // without the concatenation character.
    public long timeDeltaMs;

    // Whether the favicon should be shown for this row. Only tab related updates show a favicon.
    public boolean showFavicon;

    // The type of action to be taken when this activity row is clicked.
    public @RecentActivityAction int action;

    // Implicit metadata that will be used to invoke the delegate when the activity row is clicked.
    public MessageAttribution activityMetadata;
}
