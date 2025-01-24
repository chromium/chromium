// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.build.annotations.NullMarked;

/**
 * Java interface that is responsible for hooking up the local tab model of an activity to the
 * {@link TabGroupSyncService} backend for syncing tab groups of both sides. Per-activity object and
 * is owned by the activity.
 *
 * <p>TODO(crbug.com/379699409): Make TabGroupUiActionHandler unnecessary and replace with
 * controller.
 *
 * <p>TODO(crbug.com/379699409): Make this class owned by the service delegate.
 */
@NullMarked
public interface TabGroupSyncController extends TabGroupUiActionHandler {
    /** Called when the activity is getting destroyed. */
    void destroy();
}
