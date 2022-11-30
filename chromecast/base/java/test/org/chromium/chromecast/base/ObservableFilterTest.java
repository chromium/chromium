// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for Observable#filter().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableFilterTest {
    @Test
    public void testFilter() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder empty = ReactiveRecorder.record(a.filter(String::isEmpty));
        ReactiveRecorder startsWithA = ReactiveRecorder.record(a.filter(s -> s.startsWith("a")));
        ReactiveRecorder endsWithA = ReactiveRecorder.record(a.filter(s -> s.endsWith("a")));

        a.set("");
        empty.verify().opened("").end();
        startsWithA.verify().end();
        endsWithA.verify().end();

        a.reset();
        empty.verify().closed("").end();
        startsWithA.verify().end();
        endsWithA.verify().end();

        a.set("a");
        empty.verify().end();
        startsWithA.verify().opened("a").end();
        endsWithA.verify().opened("a").end();

        a.set("doa");
        empty.verify().end();
        startsWithA.verify().closed("a").end();
        endsWithA.verify().closed("a").opened("doa").end();

        a.set("ada");
        empty.verify().end();
        startsWithA.verify().opened("ada").end();
        endsWithA.verify().closed("doa").opened("ada").end();
    }
}
