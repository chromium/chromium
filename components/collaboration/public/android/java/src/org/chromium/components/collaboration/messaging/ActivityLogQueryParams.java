// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;

/**
 * Query params for retrieving a list of rows to be shown on the activity log UI.
 * TODO: Add a proper constructor to avoid @SuppressWarnings("NullAway.Init")
*/
@NullMarked
public class ActivityLogQueryParams {
    /** The collaboration ID associated with the activity log. */
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String collaborationId;
}
