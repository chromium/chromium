// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.data_sharing.mojom.LogSource;

/**
 * Helper class to facilitate storing Data Sharing logs and exposing them to
 * chrome://data-sharing-internals.
 */
@NullMarked
public interface Logger {
    /**
     * Stores a log entry to be viewed later in chrome://data-sharing-internals. May do nothing if
     * the logging system is disabled.
     *
     * @param source A {@link LogSource} entry detailing what subsystem the log came from.
     * @param message The specific message to log.
     */
    void log(@LogSource.EnumType int source, String message);
}
