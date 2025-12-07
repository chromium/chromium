// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * Struct containing the info of ChromeTabbedActivity instance needed to manage multi-instance
 * support on Android S.
 */
@NullMarked
public final class InstanceInfo {
    /** Type of the instance necessary for UI. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Type.CURRENT, Type.ADJACENT, Type.OTHER})
    public @interface Type {
        int CURRENT = 1; // Current, active instance.
        int ADJACENT = 2; // Instance running in the adjacent window.
        int OTHER = 3; // Hidden or not yet instantiated.
    }

    /**
     * ID of ChromeTabbedActivity instance. This is compatible with the index used for persistent
     * tab state disk file, appended at the end of the file name (such as tab_state0).
     */
    public final int instanceId;

    /** ID of a task containing the activity. */
    public final int taskId;

    /** {@link Type} of an instance. */
    public final @Type int type;

    /** URL of the currently visible tab of an instance. */
    public final String url;

    /** Default title for the entry shown on UI for an instance, if custom title does not exist. */
    public final String title;

    /** Custom title for the entry shown on UI for an instance . */
    @Nullable public final String customTitle;

    /** The number of normal tabs of an instance. */
    public final int tabCount;

    /** The number of incognito tabs of an instance. */
    public final int incognitoTabCount;

    /** {@code true} if the active tab is of incognito type */
    public final boolean isIncognitoSelected;

    /**
     * The timestamp (in ms) reflecting the last time the instance was accessed. The timestamp is
     * recorded when the instance is created and when its top resumed activity status changes.
     */
    public final long lastAccessedTime;

    /**
     * Whether this instance was closed by an explicit user request. This is relevant only for a
     * non-active instance.
     */
    public final boolean closedByUser;

    public InstanceInfo(
            int instanceId,
            int taskId,
            @Type int type,
            String url,
            String title,
            @Nullable String customTitle,
            int tabCount,
            int incognitoTabCount,
            boolean isIncognitoSelected,
            long lastAccessedTime,
            boolean closedByUser) {
        this.instanceId = instanceId;
        this.taskId = taskId;
        this.type = type;
        this.url = url;
        this.title = title;
        this.customTitle = customTitle;
        this.tabCount = tabCount;
        this.incognitoTabCount = incognitoTabCount;
        this.isIncognitoSelected = isIncognitoSelected;
        this.lastAccessedTime = lastAccessedTime;
        this.closedByUser = closedByUser;
    }

    @Override
    public String toString() {
        // TODO (crbug.com/414676281): Add lastAccessedTime as "time ago" string.
        return String.format(
                Locale.ENGLISH,
                "(%d,%3d) incognito: %s ntab:%d itab:%d (%s,%s)",
                instanceId,
                taskId,
                isIncognitoSelected ? "O" : "-",
                tabCount,
                incognitoTabCount,
                title,
                url);
    }
}
