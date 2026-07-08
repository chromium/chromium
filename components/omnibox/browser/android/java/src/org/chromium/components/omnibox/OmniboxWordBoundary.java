// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Computes word-deletion boundaries for the Omnibox. */
@NullMarked
public class OmniboxWordBoundary {
    private OmniboxWordBoundary() {}

    /**
     * Returns the offset a word-granularity delete starting at {@code cursor} should extend to.
     *
     * @param text The text being edited.
     * @param cursor The current (collapsed) cursor offset within {@code text}.
     * @param forward True for a forward delete; false for backward.
     * @return The boundary offset; equal to {@code cursor} when there is nothing to delete.
     */
    public static int getDeletionBoundary(String text, int cursor, boolean forward) {
        return OmniboxWordBoundaryJni.get().getDeletionBoundary(text, cursor, forward);
    }

    @NativeMethods
    public interface Natives {
        int getDeletionBoundary(
                @JniType("std::u16string") String text, int cursor, boolean forward);
    }
}
