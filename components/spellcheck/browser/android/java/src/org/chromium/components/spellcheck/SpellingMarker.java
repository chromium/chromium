// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.spellcheck;

import org.chromium.build.annotations.NullMarked;

import java.util.Locale;
import java.util.Objects;

/** A class to represent a spelling marker, i.e. Grammar and Spelling markers. */
@NullMarked
public class SpellingMarker {
    // LINT.IfChange(SpellCheckDecoration)
    /** Values from spellcheck::Decoration on the C++ side * */
    public @interface Decoration {
        public static final int SPELLING = 0;
        public static final int GRAMMAR = 1;
    }

    // LINT.ThenChange(/components/spellcheck/common/spellcheck_decoration.h:DecorationEnum)

    private final int mStart;
    private final int mEnd;
    private final @Decoration int mType;

    public SpellingMarker(int start, int end, @Decoration int type) {
        if (start < 0) {
            throw new IllegalArgumentException("start cannot be negative");
        }
        if (end < 0) {
            throw new IllegalArgumentException("end cannot be negative");
        }
        if (start > end) {
            throw new IllegalArgumentException("start cannot be greater than end");
        }
        mStart = start;
        mEnd = end;
        mType = type;
    }

    public int start() {
        return mStart;
    }

    public int end() {
        return mEnd;
    }

    public @Decoration int type() {
        return mType;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof SpellingMarker)) return false;
        if (o == this) return true;
        SpellingMarker r = (SpellingMarker) o;
        return mStart == r.mStart && mEnd == r.mEnd && mType == r.mType;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.ENGLISH, "SpellingMarker(start=%d, end=%d, type=%d)", mStart, mEnd, mType);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mStart, mEnd, mType);
    }
}
