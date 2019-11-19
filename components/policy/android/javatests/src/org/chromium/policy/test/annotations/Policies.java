// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy.test.annotations;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.junit.Assert;

import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.policy.AbstractAppRestrictionsProvider;
import org.chromium.policy.test.PolicyData;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

/**
 * Annotations and utilities for testing code dependent on policies.
 *
 * Usage example:
 * <pre>
 * @Policies.Add({
 *     @Policies.Item(key="Foo", string="Bar"),
 *     @Policies.Item(key="Baz", stringArray={"Baz"})
 * })
 * public class MyTestClass extends BaseActivityInstrumentationTestCase<ContentActivity> {
 *
 *     public void MyTest1() {
 *         // Will run the Foo and Bar policies set
 *     }
 *
 *     @Policies.Remove(@Policies.Item(key="Baz"))
 *     public void MyTest2() {
 *         // Will run with only the Foo policy set
 *     }
 * }
 * </pre>
 */
public final class Policies {
    /** Items declared here will be added to the list of used policies. */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    public @interface Add {
        Item[] value();
    }

    /** Items declared here will be removed from the list of used policies. */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    public @interface Remove {
        Item[] value();
    }

    /**
     * Individual policy item. Identified by a {@link #key}, and optional data values.
     * At most one value argument (e.g. {@link #string()}, {@link #stringArray()}) can be used. A
     * test failure will be caused otherwise.
     */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    public @interface Item {
        String key();

        String string() default "";

        String[] stringArray() default {};
    }

    private Policies() {
        throw new AssertionError("Policies is a non-instantiable class");
    }

    /** Parses the annotations to extract usable information as {@link PolicyData} objects. */
    private static Map<String, PolicyData> fromItems(Item[] items) {
        Map<String, PolicyData> result = new HashMap<>();
        for (Item item : items) {
            PolicyData data = null;

            if (!item.string().isEmpty()) {
                Assert.assertNull("There can be at most one type of value for the policy", data);
                data = new PolicyData.Str(item.key(), item.string());
            }

            if (item.stringArray().length != 0) {
                Assert.assertNull("There can be at most one type of value for the policy", data);
                data = new PolicyData.StrArray(item.key(), item.stringArray());
            }

            if (data == null) data = new PolicyData.Undefined(item.key());
            result.put(data.getKey(), data);
        }
        return result;
    }

    /** @see PreTestHook */
    public static PreTestHook getRegistrationHook() {
        return new RegistrationHook();
    }

    @VisibleForTesting
    static Map<String, PolicyData> getPolicies(AnnotatedElement element) {
        AnnotatedElement parent = (element instanceof Method)
                ? ((Method) element).getDeclaringClass()
                : ((Class<?>) element).getSuperclass();
        Map<String, PolicyData> flags = (parent == null)
                ? new HashMap<String, PolicyData>()
                : getPolicies(parent);

        if (element.isAnnotationPresent(Policies.Add.class)) {
            flags.putAll(fromItems(element.getAnnotation(Policies.Add.class).value()));
        }

        if (element.isAnnotationPresent(Policies.Remove.class)) {
            flags.keySet().removeAll(
                    fromItems(element.getAnnotation(Policies.Remove.class).value()).keySet());
        }

        return flags;
    }

    /**
     * Registration hook for the {@link Policies} annotation family. Before a test, will parse
     * the declared policies and use them as cached policies.
     */
    public static class RegistrationHook implements PreTestHook {
        @Override
        public void run(Context targetContext, Method testMethod) {
            Map<String, PolicyData> policyMap = getPolicies(testMethod);
            if (policyMap.isEmpty()) {
                AbstractAppRestrictionsProvider.setTestRestrictions(null);
            } else {
                final Bundle policyBundle = PolicyData.asBundle(policyMap.values());
                AbstractAppRestrictionsProvider.setTestRestrictions(policyBundle);
            }
        }
    }
}
