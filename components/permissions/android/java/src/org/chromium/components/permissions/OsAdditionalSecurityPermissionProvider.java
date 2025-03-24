// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;

/**
 * Placeholder provider class to query whether the operating system has granted various security
 * permissions.
 */
@NullMarked
public abstract class OsAdditionalSecurityPermissionProvider {
    /**
     * Returns whether the operating system has granted permission to enable javascript optimizers.
     * Implementations must allow querying from any thread.
     */
    public abstract boolean hasJavascriptOptimizerPermission();

    /**
     * Returns message to display in site settings explaining why the operating system has denied
     * the javascript-optimizer permission.
     */
    public String getJavascriptOptimizerMessage(Context context) {
        return "";
    }
}
