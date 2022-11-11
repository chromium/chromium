// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/**
 * Tests for OwnedScope.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(BlockJUnit4ClassRunner.class)
public class OwnedScopeTest {
    @Test
    public void closeWithoutSetting() {
        OwnedScope scope = new OwnedScope();
        scope.close();
        scope.close(); // This should be not throw any exceptions.
    }

    @Test
    public void closeAfterSetting() {
        Box<String> state = new Box<>("");
        OwnedScope scope = new OwnedScope(() -> state.value = "a");
        assertEquals(state.value, "");
        // set() closes the scope passed in the constructor, if one was passed.
        scope.set(() -> state.value = "b");
        assertEquals(state.value, "a");
        // set() closes the inner scope when set() has previously been called.
        scope.set(() -> state.value += "*");
        assertEquals(state.value, "b");
        // close() closes the inner scope.
        scope.close();
        assertEquals(state.value, "b*");
        // Second close is a no-op.
        scope.close();
        assertEquals(state.value, "b*");
    }

    @Test
    public void setSameScopeTwiceIsIdempotent() {
        Box<String> state = new Box<>("");
        Scope scope = () -> state.value += "x";
        OwnedScope ownedScope = new OwnedScope(scope);
        assertEquals(state.value, "");
        ownedScope.set(scope);
        assertEquals(state.value, "");
        ownedScope.set(scope);
        assertEquals(state.value, "");
        ownedScope.close();
        assertEquals(state.value, "x");
    }
}
