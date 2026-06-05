// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.TextSelection;

/** Unit tests for {@link AutocompleteState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteStateUnitTest {
    @Test
    public void testIsProperSubstring() {
        assertTrue(AutocompleteState.isPrefix("ab", "abc"));

        assertFalse(AutocompleteState.isPrefix("ab", "ab"));
        assertFalse(AutocompleteState.isPrefix("abc", "ab"));
    }

    @Test
    public void testIsForwardTyped() {
        assertTrue(
                new AutocompleteState("abc", "de", null, new TextSelection(3, 3), null)
                        .isForwardTypedFrom(
                                new AutocompleteState(
                                        "ab", "c", null, new TextSelection(2, 2), null)));
        assertTrue(
                new AutocompleteState("abcd", "e", null, new TextSelection(4, 4), null)
                        .isForwardTypedFrom(
                                new AutocompleteState(
                                        "ab", "c", null, new TextSelection(2, 2), null)));

        assertFalse(
                new AutocompleteState("abc", "de", null, new TextSelection(3, 3), null)
                        .isForwardTypedFrom(
                                new AutocompleteState(
                                        "abc", "", null, new TextSelection(3, 3), null)));
        assertFalse(
                new AutocompleteState("abc", "de", null, new TextSelection(3, 3), null)
                        .isForwardTypedFrom(
                                new AutocompleteState(
                                        "abcd", "e", null, new TextSelection(4, 4), null)));
    }

    @Test
    public void testGetBackwardDeletedTextFrom() {
        assertEquals(
                "c",
                new AutocompleteState("ab", "", null, new TextSelection(2, 2), null)
                        .getBackwardDeletedTextFrom(
                                new AutocompleteState(
                                        "abc", "d", null, new TextSelection(3, 3), null)));
        // A string is not seen as backward deleted from itself.
        assertEquals(
                null,
                new AutocompleteState("ab", "", null, new TextSelection(2, 2), null)
                        .getBackwardDeletedTextFrom(
                                new AutocompleteState(
                                        "ab", "d", null, new TextSelection(2, 2), null)));
        // Reversed.
        assertEquals(
                null,
                new AutocompleteState("abc", "", null, new TextSelection(3, 3), null)
                        .getBackwardDeletedTextFrom(
                                new AutocompleteState(
                                        "ab", "d", null, new TextSelection(2, 2), null)));
        // Selection at the end is valid.
        assertEquals(
                "c",
                new AutocompleteState("ab", "", null, new TextSelection(2, 2), null)
                        .getBackwardDeletedTextFrom(
                                new AutocompleteState(
                                        "abc", "d", null, new TextSelection(2, 3), null)));
        assertNull(
                new AutocompleteState("ab", "", null, new TextSelection(2, 3), null)
                        .getBackwardDeletedTextFrom(
                                new AutocompleteState(
                                        "abc", "d", null, new TextSelection(2, 2), null)));
    }

    private void verifyReuseAutocompleteText(
            AutocompleteState s1,
            AutocompleteState s2,
            boolean expectedRetVal,
            String expectedAutocompleteText) {
        assertEquals(expectedRetVal, s2.reuseAutocompleteTextIfPrefixExtension(s1));
        assertEquals(expectedAutocompleteText, s2.getAutocompleteText());
    }

    @Test
    public void testReuseAutocompleteText() {
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "cd", null, new TextSelection(2, 2), null),
                new AutocompleteState("abc", "", null, new TextSelection(3, 3), null),
                true,
                "d");
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "dc", null, new TextSelection(2, 2), null),
                new AutocompleteState("ab", "", null, new TextSelection(2, 2), null),
                true,
                "dc");

        // The new state cannot reuse autocomplete text.
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "dc", null, new TextSelection(2, 2), null),
                new AutocompleteState("abc", "a", null, new TextSelection(3, 3), null),
                false,
                "a");
        // There is no autocomplete text to start with.
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "", null, new TextSelection(2, 2), null),
                new AutocompleteState("ab", "a", null, new TextSelection(3, 3), null),
                false,
                "a");
    }

    @Test
    public void testNullAutocompleteText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", null, null, new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAutocompleteText());
    }

    @Test
    public void testEmptyAutocompleteText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", "", "", new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAutocompleteText());
    }

    @Test
    public void testSetAutocompleteText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", null, null, new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAutocompleteText());

        autocompleteState.setAutocompleteText("def");
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abcdef", autocompleteState.getText());
        assertNotNull(autocompleteState.getAutocompleteText());
        assertEquals("def", autocompleteState.getAutocompleteText());

        autocompleteState.setAutocompleteText(null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAutocompleteText());
    }

    @Test
    public void testAdditionalText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", null, "foo.com", new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNotNull(autocompleteState.getAdditionalText());
        assertEquals("foo.com", autocompleteState.getAdditionalText());
    }

    @Test
    public void testNullAdditionalText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", null, null, new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAdditionalText());
    }

    @Test
    public void testEmptyAdditionalText() {
        AutocompleteState autocompleteState =
                new AutocompleteState("abc", "", "", new TextSelection(3, 3), null);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertNull(autocompleteState.getAdditionalText());
    }

    @Test
    public void testIsWholeUserTextSelected() {
        // LTR selection
        assertTrue(
                new AutocompleteState("abc", null, null, new TextSelection(0, 3), null)
                        .isWholeUserTextSelected());
        // RTL (reverse) selection
        assertTrue(
                new AutocompleteState("abc", null, null, new TextSelection(3, 0), null)
                        .isWholeUserTextSelected());
        // SELECT_ALL constant
        assertTrue(
                new AutocompleteState("abc", null, null, TextSelection.SELECT_ALL, null)
                        .isWholeUserTextSelected());
        // Partial selection
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(1, 3), null)
                        .isWholeUserTextSelected());
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(3, 1), null)
                        .isWholeUserTextSelected());
    }

    @Test
    public void testIsCursorAtEndOfUserText() {
        // SELECT_END ⮕ true
        assertTrue(
                new AutocompleteState("abc", null, null, TextSelection.SELECT_END, null)
                        .isCursorAtEndOfUserText());
        // SELECT_ALL ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, TextSelection.SELECT_ALL, null)
                        .isCursorAtEndOfUserText());
        // 0 .. text.length() ⮕ true
        assertTrue(
                new AutocompleteState("abc", null, null, new TextSelection(0, 3), null)
                        .isCursorAtEndOfUserText());
        // text.length() .. 0 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(3, 0), null)
                        .isCursorAtEndOfUserText());
        // text.length() - 1 .. text.length() ⮕ true
        assertTrue(
                new AutocompleteState("abc", null, null, new TextSelection(2, 3), null)
                        .isCursorAtEndOfUserText());
        // text.length() .. text.length() - 1 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(3, 2), null)
                        .isCursorAtEndOfUserText());
        // 0 .. text.length() - 1 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(0, 2), null)
                        .isCursorAtEndOfUserText());
        // text.length() - 1 .. 0 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(2, 0), null)
                        .isCursorAtEndOfUserText());
        // 1 .. text.length() - 1 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(1, 2), null)
                        .isCursorAtEndOfUserText());
        // text.length() - 1 .. 1 ⮕ false
        assertFalse(
                new AutocompleteState("abc", null, null, new TextSelection(2, 1), null)
                        .isCursorAtEndOfUserText());
        // INVALID
        assertFalse(
                new AutocompleteState("abc", null, null, TextSelection.INVALID, null)
                        .isCursorAtEndOfUserText());
    }
}
