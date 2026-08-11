// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assertNonNull;

import android.text.Editable;
import android.text.InputFilter;

import org.chromium.build.annotations.NullMarked;

/**
 * Utility class to temporarily strip input filters from an {@link Editable} to bypass undo history
 * recording during programmatic edits.
 *
 * <p>Any block of code performing edits that should be excluded from the undo history must be
 * wrapped in a try-with-resources block using {@code new UndoStackCapture(editable)} to ensure
 * filters are reliably restored.
 */
@NullMarked
public class UndoStackCapture implements AutoCloseable {
    private static final InputFilter[] NO_FILTERS = new InputFilter[0];

    private final Editable mEditable;
    private final InputFilter[] mOriginalFilters;

    /**
     * Temporarily strips all filters from the given {@link Editable}. They will be restored when
     * {@link #close()} is called.
     *
     * <p>Usage:
     *
     * <pre>
     * try (var capture = new UndoStackCapture(editable)) {
     *     // perform edits
     * }
     * </pre>
     *
     * @param editable the Editable to strip filters from.
     */
    public UndoStackCapture(Editable editable) {
        mEditable = editable;
        mOriginalFilters = assertNonNull(editable.getFilters());
        mEditable.setFilters(NO_FILTERS);
    }

    @Override
    public void close() {
        mEditable.setFilters(mOriginalFilters);
    }
}
