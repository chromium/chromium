// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Optional;

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
                new AutocompleteState("abc", "de", 3, 3)
                        .isForwardTypedFrom(new AutocompleteState("ab", "c", 2, 2)));
        assertTrue(
                new AutocompleteState("abcd", "e", 4, 4)
                        .isForwardTypedFrom(new AutocompleteState("ab", "c", 2, 2)));

        assertFalse(
                new AutocompleteState("abc", "de", 3, 3)
                        .isForwardTypedFrom(new AutocompleteState("abc", "", 3, 3)));
        assertFalse(
                new AutocompleteState("abc", "de", 3, 3)
                        .isForwardTypedFrom(new AutocompleteState("abcd", "e", 4, 4)));
    }

    @Test
    public void testGetBackwardDeletedTextFrom() {
        assertEquals(
                "c",
                new AutocompleteState("ab", "", 2, 2)
                        .getBackwardDeletedTextFrom(new AutocompleteState("abc", "d", 3, 3)));
        // A string is not seen as backward deleted from itself.
        assertEquals(
                null,
                new AutocompleteState("ab", "", 2, 2)
                        .getBackwardDeletedTextFrom(new AutocompleteState("ab", "d", 2, 2)));
        // Reversed.
        assertEquals(
                null,
                new AutocompleteState("abc", "", 3, 3)
                        .getBackwardDeletedTextFrom(new AutocompleteState("ab", "d", 2, 2)));
        // Selection not valid.
        assertNull(
                new AutocompleteState("ab", "", 2, 2)
                        .getBackwardDeletedTextFrom(new AutocompleteState("abc", "d", 2, 3)));
        assertNull(
                new AutocompleteState("ab", "", 2, 3)
                        .getBackwardDeletedTextFrom(new AutocompleteState("abc", "d", 2, 2)));
    }

    private void verifyReuseAutocompleteText(
            AutocompleteState s1,
            AutocompleteState s2,
            boolean expectedRetVal,
            String expectedAutocompleteText) {
        assertEquals(expectedRetVal, s2.reuseAutocompleteTextIfPrefixExtension(s1));
        assertEquals(expectedAutocompleteText, s2.getAutocompleteText().get());
    }

    @Test
    public void testReuseAutocompleteText() {
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "cd", 2, 2),
                new AutocompleteState("abc", "", 3, 3),
                true,
                "d");
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "dc", 2, 2),
                new AutocompleteState("ab", "", 2, 2),
                true,
                "dc");

        // The new state cannot reuse autocomplete text.
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "dc", 2, 2),
                new AutocompleteState("abc", "a", 3, 3),
                false,
                "a");
        // There is no autocomplete text to start with.
        verifyReuseAutocompleteText(
                new AutocompleteState("ab", "", 2, 2),
                new AutocompleteState("ab", "a", 3, 3),
                false,
                "a");
    }

    @Test
    public void testNullAutocompleteText() {
        AutocompleteState autocompleteState = new AutocompleteState("abc", null, 3, 3);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertFalse(autocompleteState.getAutocompleteText().isPresent());
    }

    @Test
    public void testEmptyAutocompleteText() {
        AutocompleteState autocompleteState = new AutocompleteState("abc", "", 3, 3);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertFalse(autocompleteState.getAutocompleteText().isPresent());
    }

    @Test
    public void testSetAutocompleteText() {
        AutocompleteState autocompleteState = new AutocompleteState("abc", null, 3, 3);
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertFalse(autocompleteState.getAutocompleteText().isPresent());

        autocompleteState.setAutocompleteText(Optional.of("def"));
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abcdef", autocompleteState.getText());
        assertTrue(autocompleteState.getAutocompleteText().isPresent());
        assertEquals("def", autocompleteState.getAutocompleteText().get());

        autocompleteState.setAutocompleteText(Optional.empty());
        assertEquals("abc", autocompleteState.getUserText());
        assertEquals("abc", autocompleteState.getText());
        assertFalse(autocompleteState.getAutocompleteText().isPresent());
    }
}
