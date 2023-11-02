// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for Observable#after().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableAfterTest {
    @Test
    public void testAfterOnlyNotifiesStateAfterSubscription() {
        Controller<String> src = new Controller<>();
        src.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(src.after());
        // Despite |src| having data, the observer is not notified.
        recorder.verify().end();
        // The observer is notified of changes after subscription.
        src.set("b");
        recorder.verify().opened("b").end();
        src.set("c");
        recorder.verify().closed("b").opened("c").end();
        recorder.unsubscribe();
        recorder.verify().closed("c").end();
    }

    @Test
    public void testAfterChained() {
        Controller<String> src = new Controller<>();
        // after() is idempotent: it shouldn't behave differently if chained multiple times.
        ReactiveRecorder recorder = ReactiveRecorder.record(src.after().after());
        recorder.verify().end();
        src.set("b");
        recorder.verify().opened("b").end();
        src.set("c");
        recorder.verify().closed("b").opened("c").end();
        recorder.unsubscribe();
        recorder.verify().closed("c").end();
    }
}
