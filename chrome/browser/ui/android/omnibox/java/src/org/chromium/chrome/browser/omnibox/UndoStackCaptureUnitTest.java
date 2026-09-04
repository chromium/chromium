// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import android.text.InputFilter;
import android.text.SpannableStringBuilder;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;

/** Unit tests for {@link UndoStackCapture}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class UndoStackCaptureUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private InputFilter mFilter1;
    @Mock private InputFilter mFilter2;

    @Test
    public void testCapture_stripsAndRestores() {
        SpannableStringBuilder editable = new SpannableStringBuilder("test");
        InputFilter[] originalFilters = new InputFilter[] {mFilter1, mFilter2};
        editable.setFilters(originalFilters);

        try (var capture = new UndoStackCapture(editable)) {
            assertEquals(0, editable.getFilters().length);
        }

        InputFilter[] restoredFilters = editable.getFilters();
        assertEquals(2, restoredFilters.length);
        assertSame(mFilter1, restoredFilters[0]);
        assertSame(mFilter2, restoredFilters[1]);
    }

    @Test
    public void testCapture_noFilters() {
        SpannableStringBuilder editable = new SpannableStringBuilder("test");
        editable.setFilters(new InputFilter[0]);

        try (var capture = new UndoStackCapture(editable)) {
            assertEquals(0, editable.getFilters().length);
        }

        assertEquals(0, editable.getFilters().length);
    }

    @Test
    public void testCapture_nested() {
        SpannableStringBuilder editable = new SpannableStringBuilder("test");
        InputFilter[] originalFilters = new InputFilter[] {mFilter1};
        editable.setFilters(originalFilters);

        try (var outer = new UndoStackCapture(editable)) {
            assertEquals(0, editable.getFilters().length);

            try (var inner = new UndoStackCapture(editable)) {
                assertEquals(0, editable.getFilters().length);
            }

            assertEquals(0, editable.getFilters().length); // Still stripped after inner close
        }

        assertEquals(1, editable.getFilters().length); // Restored after outer close
        assertSame(mFilter1, editable.getFilters()[0]);
    }
}
