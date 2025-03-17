// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;
import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;

/** Tests for Both. */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BothTest {
    @Test
    public void testAccessMembersOfBoth() {
        Both<Integer, Boolean> x = Both.of(10, true);
        assertEquals(10, (int) x.first);
        assertTrue((boolean) x.second);
    }

    @Test
    public void testDeeplyNestedBothType() {
        // Yes you can do this.
        Both<Both<Both<String, String>, String>, String> x =
                Both.of(Both.of(Both.of("A", "B"), "C"), "D");
        assertEquals("A", x.first.first.first);
        assertEquals("B", x.first.first.second);
        assertEquals("C", x.first.second);
        assertEquals("D", x.second);
    }

    @Test
    public void testBothToString() {
        Both<String, String> x = Both.of("a", "b");
        assertEquals("a, b", x.toString());
    }

    @SuppressWarnings("JUnitIncompatibleType")
    @Test
    public void testBothEquals() {
        assertEquals(Both.of("a", "b"), Both.of("a", "b"));
        assertNotEquals(Both.of("a", "b"), Both.of("b", "b"));
        assertNotEquals(Both.of("a", "b"), Both.of("a", "a"));
        assertNotEquals(Both.of(1, 2), Both.of("a", "b"));
        assertNotEquals(Both.of("hi", 0), new Object());
    }

    @Test
    public void testUseGetFirstAsMethodReference() {
        Both<Integer, String> x = Both.of(1, "one");
        Function<Both<Integer, String>, Integer> getFirst = Both::getFirst;
        assertEquals(1, (int) getFirst.apply(x));
    }

    @Test
    public void testUseGetSecondAsMethodReference() {
        Both<Integer, String> x = Both.of(2, "two");
        Function<Both<Integer, String>, String> getSecond = Both::getSecond;
        assertEquals("two", getSecond.apply(x));
    }

    @Test
    public void testAdaptBiFunction() {
        String result = Both.adapt((String a, String b) -> a + b).apply(Both.of("a", "b"));
        assertEquals("ab", result);
    }

    @Test
    public void testAdaptBiFunctionBaseArguments() {
        // Compile error if generics are wrong.
        Function<Both<Derived, Derived>, String> f = Both.adapt((Base a, Base b) -> "success");
        assertEquals("success", f.apply(Both.of(new Derived(), new Derived())));
    }

    @Test
    public void testAdaptBiFunctionDerivedResult() {
        // Compile error if generics are wrong.
        Derived derived = new Derived();
        Function<Both<String, String>, Base> f = Both.adapt((String a, String b) -> derived);
        assertEquals(derived, f.apply(Both.of("a", "b")));
    }

    @Test
    public void testAdaptBiConsumer() {
        List<String> result = new ArrayList<>();
        Both.adapt(
                        (String a, String b) -> {
                            result.add(a + b);
                        })
                .accept(Both.of("A", "B"));
        assertThat(result, contains("AB"));
    }

    @Test
    public void testAdaptBiConsumerBaseArguments() {
        // Compile error if generics are wrong.
        List<String> result = new ArrayList<>();
        Consumer<Both<Derived, Derived>> c =
                Both.adapt(
                        (Base a, Base b) -> {
                            result.add("success");
                        });
        c.accept(Both.of(new Derived(), new Derived()));
        assertThat(result, contains("success"));
    }

    @Test
    public void testAdaptBiPredicate() {
        Predicate<Both<String, String>> p = Both.adapt(String::equals);
        assertTrue(p.test(Both.of("a", "a")));
        assertFalse(p.test(Both.of("a", "b")));
    }

    @Test
    public void testAdaptBiPredicateBaseArguments() {
        // Compile error if generics are wrong.
        Predicate<Both<Derived, Derived>> p = Both.adapt((Base a, Base b) -> a.equals(b));
        Derived derived1 = new Derived();
        Derived derived2 = new Derived();
        assertTrue(p.test(Both.of(derived1, derived1)));
        assertTrue(p.test(Both.of(derived2, derived2)));
        assertFalse(p.test(Both.of(derived1, derived2)));
        assertFalse(p.test(Both.of(derived2, derived1)));
    }
}
