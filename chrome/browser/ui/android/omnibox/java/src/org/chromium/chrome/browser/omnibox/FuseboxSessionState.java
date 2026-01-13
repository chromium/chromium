// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;

import java.util.Objects;

/**
 * Fusebox / Omnibox session state object. Captures controllers and state details needed to fulfill
 * or reconstruct the user input.
 *
 * <p>The logic should be kept to minimum in this class.
 */
@NullMarked
public class FuseboxSessionState {
    @Override
    public int hashCode() {
        return Objects.hash(this);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof FuseboxSessionState)) return false;
        // The object is currently empty.
        return true;
    }
}
