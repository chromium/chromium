// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Tests for Itertools.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ItertoolsTest {
    @Test
    public void testForEachLoopWithIterator() {
        Iterator<String> emptyIterator = new Iterator<String>() {
            @Override
            public boolean hasNext() {
                return false;
            }

            @Override
            public String next() {
                throw new IllegalStateException();
            }
        };
        List<String> result = new ArrayList<>();
        // The following won't compile because for-each loops expect Iterables.
        // for (String item : emptyIterator) {
        //     result.add(item);
        // }
        for (String item : Itertools.fromIterator(emptyIterator)) {
            result.add(item);
        }
        assertThat(result, emptyIterable());
    }
}
