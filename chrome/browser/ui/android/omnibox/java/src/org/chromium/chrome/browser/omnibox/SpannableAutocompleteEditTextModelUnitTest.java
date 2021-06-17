// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Unit tests for {@link SpannableAutocompleteEditTextModel}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class SpannableAutocompleteEditTextModelUnitTest {
    @Test
    public void testNonCompositionalText() {
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("http://123.com"));
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("goo"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네이버"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("123네이버"));
    }
}
