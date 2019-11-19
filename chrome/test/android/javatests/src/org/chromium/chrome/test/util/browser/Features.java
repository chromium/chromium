// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.AnnotationRule;
import org.chromium.chrome.browser.ChromeFeatureList;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Heps with setting Feature flags during tests. Relies on registering the appropriate
 * {@link Processor} rule on the test class.
 *
 * Use {@link EnableFeatures} and {@link DisableFeatures} to specify the features to register and
 * whether they should be enabled.
 *
 * Sample code:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public Features.Processor processor = new Features.JUnitProcessor();
 *
 *    &#64;Features.EnableFeatures(ChromeFeatureList.CHROME_MODERN_DESIGN)
 *    public void testFoo() { ... }
 * }
 * </pre>
 *
 * This class also offers Singleton access to enable and disable features, letting other rules
 * affect the final configuration before the start of the test.
 */
public class Features {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableFeatures {
        String[] value();
    }

    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableFeatures {
        String[] value();
    }

    private static @Nullable Features sInstance;
    private final Map<String, Boolean> mRegisteredState = new HashMap<>();

    private Features() {}

    public static Features getInstance() {
        if (sInstance == null) sInstance = new Features();
        return sInstance;
    }

    /**
     * Explicitly applies features collected so far to the command line.
     * Note: This is only valid during instrumentation tests.
     * TODO(dgn): remove once we have the compound test rule is available to enable a deterministic
     * rule execution order.
     */
    public static void ensureCommandLineIsUpToDate() {
        getInstance().applyForInstrumentation();
    }

    /** Collects the provided features to be registered as enabled. */
    public void enable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, true);
    }

    /** Collects the provided features to be registered as disabled. */
    public void disable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, false);
    }

    private void applyForJUnit() {
        ChromeFeatureList.setTestFeatures(mRegisteredState);
    }

    private void applyForInstrumentation() {
        mergeFeatureLists("enable-features", true);
        mergeFeatureLists("disable-features", false);
    }

    /**
     * Updates the reference list of features held by the CommandLine by merging it with the feature
     * state registered via this utility.
     * @param switchName Name of the command line switch that is the reference feature state.
     * @param enabled Whether the feature list being modified is the enabled or disabled one.
     */
    private void mergeFeatureLists(String switchName, boolean enabled) {
        CommandLine commandLine = CommandLine.getInstance();
        String switchValue = commandLine.getSwitchValue(switchName);
        Set<String> existingFeatures = new HashSet<>();
        if (switchValue != null) {
            Collections.addAll(existingFeatures, switchValue.split(","));
        }
        for (String additionalFeature : mRegisteredState.keySet()) {
            if (mRegisteredState.get(additionalFeature) != enabled) continue;
            existingFeatures.add(additionalFeature);
        }

        // Not really append, it puts the value in a map so we can override values that way too.
        commandLine.appendSwitchWithValue(switchName, TextUtils.join(",", existingFeatures));
    }

    /** Resets Features-related state that might persist in between tests. */
    private static void reset() {
        sInstance = null;
        ChromeFeatureList.setTestFeatures(null);
    }

    /**
     * Feature processor intended to be used in Robolectric and {@link DummyUiActivityTestCase}
     * tests. The collected feature states would be applied to {@link ChromeFeatureList}'s
     * internal test-only feature map.
     */
    public static class JUnitProcessor extends Processor {
        @Override
        protected void applyFeatures() {
            getInstance().applyForJUnit();
        }
    }

    /**
     * Feature processor intended to be used in instrumentation tests with native library, like
     * {@link ChromeBrowserTestRule}. The collected feature states would be applied to
     * {@link CommandLine}.
     */
    public static class InstrumentationProcessor extends Processor {
        @Override
        protected void applyFeatures() {
            getInstance().applyForInstrumentation();
        }
    }

    /**
     * Add this rule to tests to activate the {@link Features} annotations and choose flags
     * to enable, or get rid of exceptions when the production code tries to check for enabled
     * features.
     */
    private abstract static class Processor extends AnnotationRule {
        public Processor() {
            super(EnableFeatures.class, DisableFeatures.class);
        }

        @Override
        protected void before() {
            collectFeatures();
            applyFeatures();
        }

        @Override
        protected void after() {
            reset();
        }

        protected abstract void applyFeatures();

        private void collectFeatures() {
            for (Annotation annotation : getAnnotations()) {
                if (annotation instanceof EnableFeatures) {
                    getInstance().enable(((EnableFeatures) annotation).value());
                } else if (annotation instanceof DisableFeatures) {
                    getInstance().disable(((DisableFeatures) annotation).value());
                }
            }
        }
    }
}
