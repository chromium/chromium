// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;

/**
 * Contains information needed to show one row in the activity log UI. This is a shim layer for the
 * native representation of the object. See
 * //components/collaboration/public/messaging/activity_log.h for specific details.
 * TODO: Add a proper constructor to avoid @SuppressWarnings("NullAway.Init")
 */
@NullMarked
public class ActivityLogItem {
    // The type of collaboration event associated with the log item.
    public @CollaborationEvent int collaborationEvent;

    // Explicit display metadata to be shown in the UI.
    // Text to be shown as title of the activity log item.
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String titleText;

    // Text to be shown as description of the activity log item.
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String descriptionText;

    // Text to be shown as relative time duration (e.g. 8 hours ago) of when
    // the event happened.
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String timeDeltaText;

    // Whether the favicon should be shown for this row. Only tab related updates show a favicon.
    public boolean showFavicon;

    // The type of action to be taken when this activity row is clicked.
    public @RecentActivityAction int action;

    // Implicit metadata that will be used to invoke the delegate when the activity row is clicked.
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public MessageAttribution activityMetadata;
}
