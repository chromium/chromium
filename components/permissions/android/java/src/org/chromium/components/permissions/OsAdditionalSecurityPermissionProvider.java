// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

/**
 * Placeholder provider class to query whether the operating system has granted various security
 * permissions.
 */
public abstract class OsAdditionalSecurityPermissionProvider {
    /**
     * Returns whether the operating system has granted permission to enable javascript optimizers.
     * Implementations must allow querying from any thread.
     */
    public abstract boolean hasJavascriptOptimizerPermission();
}
