// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.chromium.build.annotations.NullMarked;

import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/** Container holding legal message lines and the callback for link clicks. */
@NullMarked
public class LegalMessage {
    /** Legal message lines. */
    public final List<LegalMessageLine> mLines;

    /** The callback invoked when a legal message link is clicked. */
    public final Consumer<String> mLink;

    /**
     * Constructs a legal message container.
     *
     * @param lines The list of legal message lines. Must not be null.
     * @param link The callback to open legal links. Must not be null.
     */
    public LegalMessage(List<LegalMessageLine> lines, Consumer<String> link) {
        mLines = Objects.requireNonNull(lines, "List of legal message lines can't be null");
        mLink = Objects.requireNonNull(link, "Link consumer can't be null");
    }

    /**
     * Constructs an empty legal message container.
     *
     * @param link The callback to open legal links. Must not be null.
     */
    public LegalMessage(Consumer<String> link) {
        this(Collections.emptyList(), link);
    }
}
