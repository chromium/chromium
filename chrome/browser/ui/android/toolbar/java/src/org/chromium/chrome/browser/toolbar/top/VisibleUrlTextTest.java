// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link VisibleUrlText}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VisibleUrlTextTest {
    @Test
    public void testIsValidVisibleTextPrefixHint() {
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint(null, null));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", null));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint(null, "foo"));

        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("", ""));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", ""));

        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "fooo"));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "foo/"));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "o/"));
        assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "oo"));

        assertTrue(VisibleUrlText.isValidVisibleTextPrefixHint("foo.com", "foo"));
        assertTrue(VisibleUrlText.isValidVisibleTextPrefixHint("foo.com", "foo.com"));
    }
}
