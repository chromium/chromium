// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link InputState}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InputStateTest {
    @Test
    public void testEqualsAndHashCode() {
        InputState state1 =
                new InputState(
                        new int[] {1, 2},
                        new int[] {3, 4},
                        new int[] {5, 6},
                        7,
                        8,
                        new int[] {9, 10},
                        new int[] {11, 12},
                        new int[] {13, 14});
        InputState state2 =
                new InputState(
                        new int[] {1, 2},
                        new int[] {3, 4},
                        new int[] {5, 6},
                        7,
                        8,
                        new int[] {9, 10},
                        new int[] {11, 12},
                        new int[] {13, 14});
        InputState state3 =
                new InputState(
                        new int[] {0, 2}, // different
                        new int[] {3, 4},
                        new int[] {5, 6},
                        7,
                        8,
                        new int[] {9, 10},
                        new int[] {11, 12},
                        new int[] {13, 14});

        assertEquals(state1, state2);
        assertEquals(state1.hashCode(), state2.hashCode());

        assertNotEquals(state1, state3);
        assertNotEquals(state1.hashCode(), state3.hashCode());
    }
}
