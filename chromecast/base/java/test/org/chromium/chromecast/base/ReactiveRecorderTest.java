// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests that assertionss of ReactiveRecorder are thrown.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ReactiveRecorderTest {
    @Test(expected = AssertionError.class)
    public void testFailEndAtStart() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set(Unit.unit());
        recorder.verify().end();
    }

    @Test(expected = AssertionError.class)
    public void testFailEndAtEnd() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set(Unit.unit());
        controller.reset();
        controller.set(Unit.unit());
        recorder.verify().opened(Unit.unit()).closed(Unit.unit()).end();
    }

    @Test(expected = AssertionError.class)
    public void testFailOpenedWrongValue() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set("actual");
        recorder.verify().opened("expected");
    }

    @Test(expected = AssertionError.class)
    public void testFailOpenedGotClosed() {
        Controller<String> controller = new Controller<>();
        controller.set("before");
        ReactiveRecorder recorder = ReactiveRecorder.record(controller).reset();
        controller.set("after");
        recorder.verify().opened("after");
    }

    @Test(expected = AssertionError.class)
    public void testFailClosedGotOpened() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set(Unit.unit());
        recorder.verify().closed(Unit.unit());
    }

    @Test(expected = AssertionError.class)
    public void testFailGetNotificationsAfterUnsubscribe() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        recorder.unsubscribe();
        controller.set("unexpected");
        recorder.verify().opened("unexpected");
    }

    @Test
    public void testHappyPath() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set(Unit.unit());
        controller.reset();
        controller.set(Unit.unit());
        controller.reset();
        recorder.verify()
                .opened(Unit.unit())
                .closed(Unit.unit())
                .opened(Unit.unit())
                .closed(Unit.unit())
                .end();
    }
}
