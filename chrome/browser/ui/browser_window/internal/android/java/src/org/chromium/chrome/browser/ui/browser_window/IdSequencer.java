// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Generates a sequence of IDs that will be unique within a process, and will not persist across app
 * restarts. ID generation and access is thread-safe.
 */
@NullMarked
final class IdSequencer {
    private static final AtomicInteger sNextId = new AtomicInteger(0);

    private IdSequencer() {}

    /** Returns the next ID in the sequence, and moves to the next one. */
    public static int next() {
        return sNextId.getAndIncrement();
    }
}
