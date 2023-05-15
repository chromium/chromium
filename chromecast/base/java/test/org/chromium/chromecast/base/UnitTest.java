// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for Unit.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class UnitTest {
    @Test
    public void testUnitIdentity() {
        // Only one instance of Unit should ever exist.
        assertTrue(Unit.unit() == Unit.unit());
    }

    @Test
    public void testUnitEqualsItself() {
        // Unit is equal to itself.
        assertEquals(Unit.unit(), Unit.unit());
    }

    @Test
    public void testUnitDoesNotEqualAnythingElse() {
        // Unit is not equal to other arbitrary objects.
        assertThat(Unit.unit(), not(new Object()));
    }
}
