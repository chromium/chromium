// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;

/** Encapsulates details of a text change in the UrlBar. */
@NullMarked
public class UrlBarTextChangeInfo {

    /** The new text after the change (without autocomplete). */
    private final String mText;

    /**
     * The offset of the start of the range of the text that was modified.
     *
     * <p>Example: "abc" -> user deletes "c" -> "ab" (mStart=2).
     */
    private final int mStart;

    /**
     * The length of the former text that has been replaced.
     *
     * <p>Example: "abc" -> selecting "c" and typing "d" over "c" -> "abd" (mBeforeLength=1).
     */
    private final int mBeforeLength;

    /**
     * The length of the replacement modified text.
     *
     * <p>Example: "abc" -> user types "d" -> "abcd" (mAfterLength=1).
     */
    private final int mAfterLength;

    public UrlBarTextChangeInfo(String text, int start, int before, int after) {
        mText = text;
        mStart = start;
        mBeforeLength = before;
        mAfterLength = after;
    }

    public String getText() {
        return mText;
    }

    public int getStart() {
        return mStart;
    }

    public int getAfter() {
        return mAfterLength;
    }

    /**
     * @return Whether the change was a pure deletion.
     *     <p>Example: "abc" -> user deletes "c" -> "ab" (start=2, beforeLength=1, afterLength=0).
     */
    public boolean isDelete() {
        return mBeforeLength > 0 && mAfterLength == 0;
    }

    /**
     * @return Whether the change was a pure insertion.
     *     <p>Example: "ab" -> user types "c" -> "abc" (start=2, beforeLength=0, afterLength=1).
     */
    public boolean isInsertion() {
        return mBeforeLength == 0 && mAfterLength > 0;
    }

    /**
     * @return Whether the change was a replacement (typing over selection).
     *     <p>Example: "abc", user selects "c" and types "d" -> "abd" (start=2, beforeLength=1,
     *     afterLength=1).
     */
    public boolean isReplacement() {
        return mBeforeLength > 0 && mAfterLength > 0;
    }
}
