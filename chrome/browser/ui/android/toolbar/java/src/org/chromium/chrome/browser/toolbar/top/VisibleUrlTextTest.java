// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link VisibleUrlText}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VisibleUrlTextTest {
    @Test
    public void testIsValidVisibleTextPrefixHint() {
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint(null, null));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", null));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint(null, "foo"));

        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("", ""));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", ""));

        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "fooo"));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "foo/"));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "o/"));
        Assert.assertFalse(VisibleUrlText.isValidVisibleTextPrefixHint("foo", "oo"));

        Assert.assertTrue(VisibleUrlText.isValidVisibleTextPrefixHint("foo.com", "foo"));
        Assert.assertTrue(VisibleUrlText.isValidVisibleTextPrefixHint("foo.com", "foo.com"));
    }
}
