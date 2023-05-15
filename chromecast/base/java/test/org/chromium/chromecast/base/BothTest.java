// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;

/**
 * Tests for Both.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class BothTest {
    @Test
    public void testAccessMembersOfBoth() {
        Both<Integer, Boolean> x = Both.both(10, true);
        assertEquals((int) x.first, 10);
        assertEquals((boolean) x.second, true);
    }

    @Test
    public void testDeeplyNestedBothType() {
        // Yes you can do this.
        Both<Both<Both<String, String>, String>, String> x =
                Both.both(Both.both(Both.both("A", "B"), "C"), "D");
        assertEquals(x.first.first.first, "A");
        assertEquals(x.first.first.second, "B");
        assertEquals(x.first.second, "C");
        assertEquals(x.second, "D");
    }

    @Test
    public void testBothToString() {
        Both<String, String> x = Both.both("a", "b");
        assertEquals(x.toString(), "a, b");
    }

    @Test
    public void testBothEquals() {
        assertTrue(Both.both("a", "b").equals(Both.both("a", "b")));
        assertFalse(Both.both("a", "b").equals(Both.both("b", "b")));
        assertFalse(Both.both("a", "b").equals(Both.both("a", "a")));
        assertFalse(Both.both(1, 2).equals(Both.both("a", "b")));
        assertFalse(Both.both("hi", 0).equals(new Object()));
    }

    @Test
    public void testUseGetFirstAsMethodReference() {
        Both<Integer, String> x = Both.both(1, "one");
        Function<Both<Integer, String>, Integer> getFirst = Both::getFirst;
        assertEquals((int) getFirst.apply(x), 1);
    }

    @Test
    public void testUseGetSecondAsMethodReference() {
        Both<Integer, String> x = Both.both(2, "two");
        Function<Both<Integer, String>, String> getSecond = Both::getSecond;
        assertEquals(getSecond.apply(x), "two");
    }

    @Test
    public void testAdaptBiFunction() {
        String result = Both.adapt((String a, String b) -> a + b).apply(Both.both("a", "b"));
        assertEquals(result, "ab");
    }

    @Test
    public void testAdaptBiFunctionBaseArguments() {
        // Compile error if generics are wrong.
        Function<Both<Derived, Derived>, String> f = Both.adapt((Base a, Base b) -> "success");
        assertEquals(f.apply(Both.both(new Derived(), new Derived())), "success");
    }

    @Test
    public void testAdaptBiFunctionDerivedResult() {
        // Compile error if generics are wrong.
        Derived derived = new Derived();
        Function<Both<String, String>, Base> f = Both.adapt((String a, String b) -> derived);
        assertEquals(f.apply(Both.both("a", "b")), derived);
    }

    @Test
    public void testAdaptBiConsumer() {
        List<String> result = new ArrayList<>();
        Both.adapt((String a, String b) -> { result.add(a + b); }).accept(Both.both("A", "B"));
        assertThat(result, contains("AB"));
    }

    @Test
    public void testAdaptBiConsumerBaseArguments() {
        // Compile error if generics are wrong.
        List<String> result = new ArrayList<>();
        Consumer<Both<Derived, Derived>> c =
                Both.adapt((Base a, Base b) -> { result.add("success"); });
        c.accept(Both.both(new Derived(), new Derived()));
        assertThat(result, contains("success"));
    }

    @Test
    public void testAdaptBiPredicate() {
        Predicate<Both<String, String>> p = Both.adapt(String::equals);
        assertTrue(p.test(Both.both("a", "a")));
        assertFalse(p.test(Both.both("a", "b")));
    }

    @Test
    public void testAdaptBiPredicateBaseArguments() {
        // Compile error if generics are wrong.
        Predicate<Both<Derived, Derived>> p = Both.adapt((Base a, Base b) -> a.equals(b));
        Derived derived1 = new Derived();
        Derived derived2 = new Derived();
        assertTrue(p.test(Both.both(derived1, derived1)));
        assertTrue(p.test(Both.both(derived2, derived2)));
        assertFalse(p.test(Both.both(derived1, derived2)));
        assertFalse(p.test(Both.both(derived2, derived1)));
    }
}
