// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class PoolTest {
    @Test
    public void emptyPool() {
        Pool<String> pool = new Pool<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(pool);
        recorder.verify().end();
    }

    @Test
    public void addAndRemove() {
        Pool<String> pool = new Pool<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(pool);
        recorder.verify().end();
        var a = pool.add("a");
        recorder.verify().opened("a").end();
        var b = pool.add("b");
        recorder.verify().opened("b").end();
        a.close();
        recorder.verify().closed("a").end();
        b.close();
        recorder.verify().closed("b").end();
    }

    @Test
    public void addAndRemove_withMultipleObservers() {
        Pool<String> pool = new Pool<>();
        ReactiveRecorder recorder1 = ReactiveRecorder.record(pool);
        ReactiveRecorder recorder2 = ReactiveRecorder.record(pool);
        recorder1.verify().end();
        recorder2.verify().end();
        var a = pool.add("a");
        recorder1.verify().opened("a").end();
        recorder2.verify().opened("a").end();
        var b = pool.add("b");
        recorder1.verify().opened("b").end();
        recorder2.verify().opened("b").end();
        a.close();
        recorder1.verify().closed("a").end();
        recorder2.verify().closed("a").end();
        b.close();
        recorder1.verify().closed("b").end();
        recorder2.verify().closed("b").end();
    }

    @Test
    public void dataRemovedInfReverseOrderWhenUnsubscribed() {
        Pool<String> pool = new Pool<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(pool);
        recorder.verify().end();
        var a = pool.add("a");
        var b = pool.add("b");
        var c = pool.add("c");
        recorder.verify().opened("a").opened("b").opened("c").end();
        recorder.unsubscribe();
        recorder.verify().closed("c").closed("b").closed("a").end();
        a.close();
        b.close();
        c.close();
        recorder.verify().end();
    }

    @Test
    public void addSameDataTwice() {
        Pool<String> pool = new Pool<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(pool);
        recorder.verify().end();
        var a1 = pool.add("a");
        // This is valid! Each activation is unique, even if the data is the same.
        var a2 = pool.add("a");
        recorder.verify().opened("a").opened("a").end();
        a1.close();
        recorder.verify().closed("a").end();
        a2.close();
        recorder.verify().closed("a").end();
    }
}
