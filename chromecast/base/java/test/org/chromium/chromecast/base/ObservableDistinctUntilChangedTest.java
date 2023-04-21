// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/**
 * Tests for Observable#distinctUntilChanged().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableDistinctUntilChangedTest {

    @Test
    public void testSingleInvocationTriggersOnce() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.distinctUntilChanged());
        a.set("foo");
        recorder.verify().opened("foo").end();
    }

    @Test
    public void testMultipleInvocationsWithSameDataOnlyTriggersOnce() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.distinctUntilChanged());
        a.set("foo");
        a.set("foo");
        a.set("foo");
        recorder.verify().opened("foo").end();
    }

    @Test
    public void testMultipleInvocationsWithNewDataTriggersMultipleTimes() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.distinctUntilChanged());
        a.set("foo");
        a.set("bar");
        a.set("foo");
        recorder.verify().opened("foo").closed("foo").opened("bar").closed("bar")
                .opened("foo").end();
    }

    @Test
    public void testMultipleInvocationsWithSameDataFollowedByNewDataTriggersOnlyForNewData() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.distinctUntilChanged());
        a.set("foo");
        a.set("foo");
        a.set("foo");
        a.set("bar");
        recorder.verify().opened("foo").closed("foo").opened("bar").end();
    }

}
