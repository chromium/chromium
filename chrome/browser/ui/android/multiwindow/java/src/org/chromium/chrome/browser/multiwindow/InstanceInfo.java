// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Struct containing the info of ChromeTabbedActivity instance needed to manage
 * multi-instance support on Android S.
 */
final class InstanceInfo {
    /**
     * Type of the instance necessary for UI.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Type.CURRENT, Type.ADJACENT, Type.OTHER})
    public @interface Type {
        int CURRENT = 1; // Current, active instance.
        int ADJACENT = 2; // Instance running in the adjacent window.
        int OTHER = 3; // Hidden or uninstantiated yet.
    }

    /**
     * ID of ChromeTabbedActivity instance. This is compatible with the index used for
     * persistent tab state disk file, appended at the end of the file name (such as tab_state0).
     */
    public final int instanceId;

    /**
     * ID of a task containing the activity.
     */
    public final int taskId;

    /**
     * {@link Type} of an instance.
     */
    public final @Type int type;

    /**
     * URL of the currently visible tab of an instance.
     */
    public final String url;

    /**
     * Title for the entry shown on UI for an instance .
     */
    public final String title;

    /**
     * The number of tabs of an instance.
     */
    public final int tabCount;

    public InstanceInfo(
            int instanceId, int taskId, @Type int type, String url, String title, int tabCount) {
        this.instanceId = instanceId;
        this.taskId = taskId;
        this.type = type;
        this.url = url;
        this.title = title;
        this.tabCount = tabCount;
    }

    @Override
    public String toString() {
        return "instance-id: " + instanceId + " task-id: " + taskId;
    }
}
