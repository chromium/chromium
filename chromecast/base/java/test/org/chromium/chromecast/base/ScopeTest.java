// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for Scope.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(BlockJUnit4ClassRunner.class)
public class ScopeTest {
    @Test
    public void constructFromLambda() {
        List<String> result = new ArrayList<>();
        Scope scope = () -> result.add("a");
        assertThat(result, emptyIterable());
        scope.close();
        assertThat(result, contains("a"));
    }

    @Test
    public void combineWithAnd() {
        List<String> result = new ArrayList<>();
        Scope a = () -> result.add("a");
        Scope b = () -> result.add("b");
        Scope ab = a.and(b);
        assertThat(result, emptyIterable());
        ab.close();
        assertThat(result, contains("b", "a"));
    }
}
