// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for Sequencer.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class SequencerTest {
    @Test
    public void testPostedTaskIsRun() {
        List<String> result = new ArrayList<>();
        Sequencer s = new Sequencer();
        s.sequence(() -> result.add("a"));
        assertThat(result, contains("a"));
    }

    @Test
    public void testTaskPostedByTaskIsRunAfterFirstTaskFinishes() {
        List<String> result = new ArrayList<>();
        Sequencer s = new Sequencer();
        s.sequence(() -> {
            s.sequence(() -> result.add("b"));
            result.add("a");
        });
        assertThat(result, contains("a", "b"));
    }

    @Test
    public void testTaskThatEnqueuesManyTasks() {
        List<Integer> result = new ArrayList<>();
        Sequencer s = new Sequencer();
        s.sequence(() -> {
            for (int i = 0; i < 10; i++) {
                final int toAdd = i;
                s.sequence(() -> result.add(toAdd));
            }
        });
        assertThat(result, contains(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
    }

    @Test(expected = Sequencer.InceptionException.class)
    public void testThrowsExceptionEventuallyOnInfiniteLoop() {
        Sequencer s = new Sequencer();
        Runnable runception = new Runnable() {
            @Override
            public void run() {
                s.sequence(this);
            }
        };
        s.sequence(runception);
    }
}
