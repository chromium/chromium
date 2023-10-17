// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test class for {@link PatternMatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PatternMatcherTest {
    @Test
    public void testPatternMatcher() throws PatternMatcher.IllegalPatternException {
        Assert.assertFalse(matchPattern("ab", "*a"));
        Assert.assertFalse(matchPattern("ab", "b*"));
        Assert.assertTrue(matchPattern("aabc", "*b*c*"));

        Assert.assertTrue(matchPattern("", ""));
        Assert.assertFalse(matchPattern("test@gmail.com", ""));

        Assert.assertTrue(matchPattern("test@gmail.com", "test@gmail.com"));
        Assert.assertFalse(matchPattern("test2@gmail.com", "test@gmail.com"));
        Assert.assertFalse(matchPattern("thetest@gmail.com", "test@gmail.com"));
        Assert.assertFalse(matchPattern("test@gmail.com.example.com", "test@gmail.com"));

        Assert.assertTrue(matchPattern("test@gmail.com", "*"));
        Assert.assertTrue(matchPattern("test@gmail.com", "*.com"));
        Assert.assertFalse(matchPattern("test@gmail.org", "*.com"));
        Assert.assertTrue(matchPattern("test@gmail.com", "*gmail.com"));
        Assert.assertTrue(matchPattern("test@thegmail.com", "*gmail.com"));
        Assert.assertFalse(matchPattern("test@gmail.com", "*@gmail.org"));
        Assert.assertFalse(matchPattern("gmail.com@example.com", "*gmail.com"));
        Assert.assertTrue(matchPattern("test@gmail.example.com", "test@*.example.com"));

        // Test escaping
        Assert.assertTrue(matchPattern("test*@gmail.com", "test\\*@gmail.com"));
        Assert.assertFalse(matchPattern("test@gmail.com", "test\\*@gmail.com"));

        Assert.assertFalse(matchPattern("test@gmail.com", "\\*"));
        Assert.assertTrue(matchPattern("\\test@gmail.com", "\\\\*"));
        // Escaping is allowed for all characters, not just asterisk
        Assert.assertTrue(matchPattern("test@gmail.com", "\\t\\e\\s\\t\\@\\gmail.com"));
    }

    @Test(expected = PatternMatcher.IllegalPatternException.class)
    public void testMalformedEscapeSequence() throws PatternMatcher.IllegalPatternException {
        // Unmatched backslash at the end of the pattern should trigger an exception.
        matchPattern("", "account@gmail.com\\");
    }

    private boolean matchPattern(String string, String pattern)
            throws PatternMatcher.IllegalPatternException {
        return new PatternMatcher(pattern).matches(string);
    }
}
