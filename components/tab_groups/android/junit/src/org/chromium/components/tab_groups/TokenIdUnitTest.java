// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_groups;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TokenId}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TokenIdUnitTest {
    @Test
    public void equals_sameTokenIdClass_sameToken_true() {
        Token token = new Token(10L, 20L);
        assertEquals(new TestTokenIdA(token), new TestTokenIdA(token));
    }

    @Test
    public void equals_sameTokenIdClass_differentTokens_false() {
        Token token1 = new Token(10L, 20L);
        Token token2 = new Token(10L, 21L);
        assertNotEquals(new TestTokenIdA(token1), new TestTokenIdA(token2));
    }

    @Test
    public void equals_differentTokenIdClasses_sameToken_false() {
        Token token = new Token(10L, 20L);
        assertNotEquals(new TestTokenIdA(token), new TestTokenIdB(token));
    }

    @Test
    public void equals_differentTokenIdClasses_differentTokens_false() {
        Token token1 = new Token(10L, 20L);
        Token token2 = new Token(30L, 40L);
        assertNotEquals(new TestTokenIdA(token1), new TestTokenIdB(token2));
    }

    private static final class TestTokenIdA extends TokenId {
        TestTokenIdA(Token token) {
            super(token);
        }
    }

    private static final class TestTokenIdB extends TokenId {
        TestTokenIdB(Token token) {
            super(token);
        }
    }
}
