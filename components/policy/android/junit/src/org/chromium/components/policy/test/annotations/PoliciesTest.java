// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test.annotations;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.components.policy.test.PolicyData;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Unit tests for the {@link Policies} annotations */
@RunWith(BlockJUnit4ClassRunner.class)
public class PoliciesTest {
    @Test
    public void testGetPolicies() throws NoSuchMethodException {
        Method method;

        // Simple element, one annotation, no parent
        assertThat(Policies.getPolicies(SomeClass.class).keySet(), is(makeSet("Ni")));

        // Simple element, removing an annotation just has no effect
        assertThat(Policies.getPolicies(SomeClassThatRemoves.class).isEmpty(), is(true));

        // Simple element, adds and removes the same element: We process additions, then removals.
        assertThat(Policies.getPolicies(SomeConfusedClass.class).isEmpty(), is(true));

        // Annotations are inherited
        method = SomeClass.class.getDeclaredMethod("someMethodWithoutWord");
        assertThat(Policies.getPolicies(method).keySet(), is(makeSet("Ni")));

        // Annotations add up
        method = SomeClass.class.getDeclaredMethod("someMethod");
        assertThat(Policies.getPolicies(method).keySet(), is(makeSet("Ni", "Neee-wom")));

        // Annotations from methods are not inherited
        method = SomeDerivedClass.class.getDeclaredMethod("someMethod");
        assertThat(Policies.getPolicies(method).keySet(), is(makeSet("Ni")));

        // Annotations are properly deduped, we get the one closest to the examined element
        method = SomeClass.class.getDeclaredMethod("someMethodThatDuplicates");
        Map<String, PolicyData> policies = Policies.getPolicies(method);
        assertThat(policies.size(), is(1));
        assertThat(policies.get("Ni"), is(instanceOf(PolicyData.Str.class)));

        // Annotations can be removed
        method = SomeClass.class.getDeclaredMethod("someMethodThatTilRecentlyHadNi");
        assertThat(
                Policies.getPolicies(method).keySet(),
                is(makeSet("Ekke Ekke Ekke Ekke Ptangya Zoooooooom Boing Ni")));
    }

    private Set<String> makeSet(String... keys) {
        return new HashSet<String>(Arrays.asList(keys));
    }

    @Policies.Add(@Policies.Item(key = "Ni"))
    private static class SomeClass {
        @SuppressWarnings("unused")
        void someMethodWithoutWord() {}

        @Policies.Add(@Policies.Item(key = "Neee-wom"))
        void someMethod() {}

        @Policies.Add(@Policies.Item(key = "Ni", string = "Makes it string, not undefined."))
        void someMethodThatDuplicates() {}

        @Policies.Remove(@Policies.Item(key = "Ni"))
        @Policies.Add(@Policies.Item(key = "Ekke Ekke Ekke Ekke Ptangya Zoooooooom Boing Ni"))
        void someMethodThatTilRecentlyHadNi() {}
    }

    private static class SomeDerivedClass extends SomeClass {
        @Override
        void someMethod() {}
    }

    @Policies.Remove(@Policies.Item(key = "Ni"))
    private static class SomeClassThatRemoves {}

    @Policies.Add(@Policies.Item(key = "Ni"))
    @Policies.Remove(@Policies.Item(key = "Ni"))
    private static class SomeConfusedClass {}
}
