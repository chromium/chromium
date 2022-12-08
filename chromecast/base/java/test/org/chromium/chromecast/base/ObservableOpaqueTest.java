// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/**
 * Tests for Observable#opaque().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableOpaqueTest {
    @Test
    public void testOpaque() {
        Controller<String> a = new Controller<>();
        Observable<?> opaque = a.opaque();

        ReactiveRecorder opaqueRecorder = ReactiveRecorder.record(opaque);
        a.set("foo");
        opaqueRecorder.verify().opened(Unit.unit()).end();

        a.reset();
        opaqueRecorder.verify().closed(Unit.unit()).end();
    }
}
