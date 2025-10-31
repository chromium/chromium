// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/** Tests for Observable.any(). */
@Batch(Batch.UNIT_TESTS)
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableAnyTest {
    @Test
    public void notActivatedAtTheStart() {
        Controller<String> src = new Controller<>();
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().end();
    }

    @Test
    public void activatedWhenSourceActivates() {
        Controller<String> src = new Controller<>();
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().end();
        src.set("foo");
        recorder.verify().opened(Unit.unit()).end();
    }

    @Test
    public void doesNotDeactivateWhenNewSourceActivationsAreAdded() {
        Pool<String> src = new Pool<>();
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().end();
        src.add("a");
        recorder.verify().opened(Unit.unit()).end();
        src.add("b");
        recorder.verify().end();
    }

    @Test
    public void doesNotDeactivateWhenNotAllSourceActivationsAreClosed() {
        Pool<String> src = new Pool<>();
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().end();
        var a = src.add("a");
        recorder.verify().opened(Unit.unit()).end();
        src.add("b");
        recorder.verify().end();
        a.close();
        recorder.verify().end();
    }

    @Test
    public void deactivatesWhenAllSourceActivationsAreClosed() {
        Pool<String> src = new Pool<>();
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().end();
        var a = src.add("a");
        recorder.verify().opened(Unit.unit()).end();
        var b = src.add("b");
        recorder.verify().end();
        a.close();
        recorder.verify().end();
        b.close();
        recorder.verify().closed(Unit.unit()).end();
    }

    @Test
    public void changesWhenOnlySourceActivationChanges() {
        Controller<String> src = new Controller<>();
        src.set("foo");
        var recorder = ReactiveRecorder.record(Observable.any(src));
        recorder.verify().opened(Unit.unit()).end();
        src.set("bar");
        recorder.verify().closed(Unit.unit()).opened(Unit.unit()).end();
    }

    @Test
    public void multipleProjectionsOfOneSource() {
        Controller<String> src = new Controller<>();
        var startsWithA = src.filter(s -> s.startsWith("a"));
        var endsWithA = src.filter(s -> s.endsWith("a"));
        var recorder = ReactiveRecorder.record(Observable.any(startsWithA.or(endsWithA)));
        recorder.verify().end();
        src.set("a");
        recorder.verify().opened(Unit.unit()).end();
        src.set("b");
        recorder.verify().closed(Unit.unit()).end();
        src.set("aa");
        recorder.verify().opened(Unit.unit()).end();
        src.set("ba");
        recorder.verify().closed(Unit.unit()).opened(Unit.unit()).end();
        src.set("ab");
        recorder.verify().closed(Unit.unit()).opened(Unit.unit()).end();
        src.set("aba");
        recorder.verify().closed(Unit.unit()).opened(Unit.unit()).end();
    }
}
