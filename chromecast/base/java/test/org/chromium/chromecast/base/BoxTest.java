// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

/** Tests for Box. */
@RunWith(BlockJUnit4ClassRunner.class)
public class BoxTest {
    @Test
    public void testValueConstructorInteger() {
        Box<Integer> box = new Box<>(10);
        assertEquals((int) box.value, 10);
    }

    @Test
    public void testValueConstructorString() {
        Box<String> box = new Box<>("hello");
        assertEquals(box.value, "hello");
    }

    @Test
    public void testCanMutateValue() {
        Box<Integer> box = new Box<>(0);
        box.value++;
        assertEquals((int) box.value, 1);
    }

    @Test
    public void testCanStoreSubclassInBoxOfSuperclass() {
        Box<Base> box = new Box<>(new Derived());
    }
}
