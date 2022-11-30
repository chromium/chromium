// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for behavior specific to Cell.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class CellTest {
    @Test
    public void defaultsToStartInteger() {
        Cell<Integer> cell = new Cell<>(10);
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened(10).end();
        r.unsubscribe();
        r.verify().closed(10).end();
    }

    @Test
    public void defaultsToStartString() {
        Cell<String> cell = new Cell<>("hello");
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened("hello").end();
        r.unsubscribe();
        r.verify().closed("hello").end();
    }

    @Test
    public void mutateOnceInteger() {
        Cell<Integer> cell = new Cell<>(0);
        ReactiveRecorder r = ReactiveRecorder.record(cell).reset();
        cell.mutate(x -> x + 1);
        r.verify().closed(0).opened(1).end();
    }

    @Test
    public void mutateOnceString() {
        Cell<String> cell = new Cell<>("a");
        ReactiveRecorder r = ReactiveRecorder.record(cell).reset();
        cell.mutate(s -> s + s);
        r.verify().closed("a").opened("aa").end();
    }

    @Test
    public void setOnceInteger() {
        Cell<Integer> cell = new Cell<>(0);
        ReactiveRecorder r = ReactiveRecorder.record(cell).reset();
        cell.set(10);
        r.verify().closed(0).opened(10).end();
    }

    @Test
    public void setOnceString() {
        Cell<String> cell = new Cell<>("a");
        ReactiveRecorder r = ReactiveRecorder.record(cell).reset();
        cell.set("b");
        r.verify().closed("a").opened("b").end();
    }

    @Test
    public void mutateMultipleInteger() {
        Cell<Integer> cell = new Cell<>(1);
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened(1).end();
        cell.mutate(x -> x + 1);
        r.verify().closed(1).opened(2).end();
        cell.mutate(x -> x + 2);
        r.verify().closed(2).opened(4).end();
        cell.mutate(x -> x - 1);
        r.verify().closed(4).opened(3).end();
    }

    @Test
    public void mutateMultipleString() {
        Cell<String> cell = new Cell<>("");
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened("").end();
        cell.mutate(s -> s + "a");
        r.verify().closed("").opened("a").end();
        cell.mutate(s -> s + "b");
        r.verify().closed("a").opened("ab").end();
        cell.mutate(s -> s + "c");
        r.verify().closed("ab").opened("abc").end();
    }

    @Test
    public void setMultipleInteger() {
        Cell<Integer> cell = new Cell<>(0);
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened(0).end();
        cell.set(1);
        r.verify().closed(0).opened(1).end();
        cell.set(2);
        r.verify().closed(1).opened(2).end();
        cell.set(3);
        r.verify().closed(2).opened(3).end();
    }

    @Test
    public void setMultipleString() {
        Cell<String> cell = new Cell<>("");
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened("").end();
        cell.set("a");
        r.verify().closed("").opened("a").end();
        cell.set("b");
        r.verify().closed("a").opened("b").end();
        cell.set("c");
        r.verify().closed("b").opened("c").end();
    }

    @Test
    public void mutateNested() {
        Cell<Integer> cell = new Cell<>(0);
        ReactiveRecorder r = ReactiveRecorder.record(cell).reset();
        cell.mutate(x -> {
            cell.mutate(y -> x + y + 2); // x = 0, y = 1
            return x + 1;
        });
        r.verify().closed(0).opened(1).closed(1).opened(3).end();
    }

    @Test
    public void dedups() {
        Cell<Integer> cell = new Cell<>(10);
        ReactiveRecorder r = ReactiveRecorder.record(cell);
        r.verify().opened(10).end();
        cell.set(10);
        r.verify().end();
    }
}
