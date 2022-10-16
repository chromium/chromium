// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.chromium.chromecast.base.Observable.empty;
import static org.chromium.chromecast.base.Observable.just;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/**
 * Tests for Observable#or().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableOrTest {
    @Test
    public void emptyOrEmpty() {
        ReactiveRecorder r = ReactiveRecorder.record(empty().or(empty()));
        r.verify().end();
    }

    @Test
    public void emptyOrJust() {
        ReactiveRecorder r = ReactiveRecorder.record(empty().or(just(10)));
        r.verify().opened(10).end();
        r.unsubscribe();
        r.verify().closed(10).end();
    }

    @Test
    public void justOrEmpty() {
        ReactiveRecorder r = ReactiveRecorder.record(just(10).or(empty()));
        r.verify().opened(10).end();
        r.unsubscribe();
        r.verify().closed(10).end();
    }

    @Test
    public void justOrJust() {
        ReactiveRecorder r = ReactiveRecorder.record(just(10).or(just(20)));
        r.verify().opened(10).opened(20).end();
        r.unsubscribe();
        r.verify().closed(20).closed(10).end();
    }

    @Test
    public void threeStrings() {
        ReactiveRecorder r = ReactiveRecorder.record(just("a").or(just("b").or(just("c"))));
        r.verify().opened("a").opened("b").opened("c").end();
        r.unsubscribe();
        r.verify().closed("c").closed("b").closed("a").end();
    }

    @Test
    public void mutableInputs() {
        Controller<String> as = new Controller<>();
        Controller<String> bs = new Controller<>();
        ReactiveRecorder r = ReactiveRecorder.record(as.or(bs));
        r.verify().end();
        as.set("a");
        r.verify().opened("a").end();
        bs.set("b");
        r.verify().opened("b").end();
        as.reset();
        r.verify().closed("a").end();
        bs.reset();
        r.verify().closed("b").end();
    }

    @Test
    public void mutableOrEmpty() {
        Controller<String> xs = new Controller<>();
        ReactiveRecorder r = ReactiveRecorder.record(xs.or(empty()));
        r.verify().end();
        xs.set("x");
        r.verify().opened("x").end();
        r.unsubscribe();
        r.verify().closed("x").end();
    }

    @Test
    public void emptyOrMutable() {
        Controller<String> xs = new Controller<>();
        ReactiveRecorder r = ReactiveRecorder.record(Observable.<String>empty().or(xs));
        r.verify().end();
        xs.set("x");
        r.verify().opened("x").end();
        r.unsubscribe();
        r.verify().closed("x").end();
    }
}
