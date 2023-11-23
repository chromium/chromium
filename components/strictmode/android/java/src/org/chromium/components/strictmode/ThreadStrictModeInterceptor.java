// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import android.os.Handler;
import android.os.Looper;
import android.os.StrictMode.ThreadPolicy;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

/** Installs a whitelist configuration for StrictMode's ThreadPolicy feature. */
public interface ThreadStrictModeInterceptor {
    /**
     * Install this interceptor and it's whitelists.
     *
     * Pre-P, this uses reflection.
     */
    void install(ThreadPolicy detectors);

    /**
     * Builds ThreadStrictModeInterceptor with the death penalty and with
     * {@link KnownViolations} exempted.
     */
    public static ThreadStrictModeInterceptor buildWithDeathPenaltyAndKnownViolationExemptions() {
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        threadInterceptor.replaceAllPenaltiesWithDeathPenalty();
        KnownViolations.addExemptions(threadInterceptor);
        return threadInterceptor.build();
    }

    /**
     * Builds a configuration for StrictMode enforcement.
     *
     * The API (but not the implementation) should stay in sync with the API used by
     * 'KnownViolations' for other apps (http://go/chrome-known-violations-upstream).
     */
    final class Builder {
        private final List<Function<Violation, Integer>> mWhitelistEntries = new ArrayList<>();
        private @Nullable Consumer<Violation> mCustomPenalty;

        /**
         * Ignores all StrictMode violations for which {@link filterPackageName} is not part of the
         * stack trace.
         *
         * Also ignores StrictMode violations where:
         * 1) {@link filterPackageName} calls {@link blocklistCalleePackageName}
         * AND
         * 2) The violation is caused in code called by {@link blocklistCalleePackageName}.
         *
         * This scenario occurs when {@link blocklistCalleePackageName} registers an observer with
         * {@link filterPackageName} and the strict mode violation is in the observer code.
         */
        public Builder onlyDetectViolationsForPackage(
                final String filterPackageName, final String blocklistCalleePackageName) {
            mWhitelistEntries.add(
                    violation -> {
                        for (StackTraceElement frame : violation.stackTrace()) {
                            if (frame.getClassName().startsWith(blocklistCalleePackageName)) {
                                return Violation.DETECT_ALL_KNOWN;
                            }
                            if (frame.getClassName().startsWith(filterPackageName)) {
                                return null;
                            }
                        }
                        return Violation.DETECT_ALL_KNOWN;
                    });
            return this;
        }

        /**
         * Ignore a violation that occurs outside of your app.
         *
         * @param violationType A mask containing one or more of the DETECT_* constants.
         * @param packageName The name of the package to ignore StrictMode violations
         *     for example, "org.chromium.foo"
         */
        public Builder ignoreExternalPackage(int violationType, final String packageName) {
            mWhitelistEntries.add(
                    violation -> {
                        if ((violation.violationType() & violationType) == 0) {
                            return null;
                        }
                        return doesStackTraceContainPackage(violation, packageName)
                                ? violationType
                                : null;
                    });
            return this;
        }

        /**
         * Returns whether the passed-in {@link Violation}'s stack trace contains a stack
         * frame within the passed-in package.
         */
        private static boolean doesStackTraceContainPackage(
                Violation violation, String packageName) {
            for (StackTraceElement frame : violation.stackTrace()) {
                if (frame.getClassName().startsWith(packageName)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * Ignore a violation that occurs outside of your app.
         *
         * @param violationType A mask containing one or more of the DETECT_* constants.
         * @param className The name of the class to ignore StrictMode violations
         *     for example, "org.chromium.foo.ThreadStrictModeInterceptor"
         */
        public Builder ignoreExternalClass(int violationType, final String className) {
            mWhitelistEntries.add(
                    violation -> {
                        if ((violation.violationType() & violationType) == 0) {
                            return null;
                        }
                        for (StackTraceElement frame : violation.stackTrace()) {
                            if (frame.getClassName().equals(className)) {
                                return violationType;
                            }
                        }
                        return null;
                    });
            return this;
        }

        /**
         * Ignore a violation that occurs outside of your app.
         *
         * @param violationType A mask containing one or more of the DETECT_* constants.
         * @param classNameWithMethod The name of the class and method to ignore StrictMode
         *         violations
         *     in. The format must be "package.Class#method", for example,
         *     "com.google.foo.ThreadStrictModeInterceptor#addAllowedMethod".
         */
        public Builder ignoreExternalMethod(int violationType, final String classNameWithMethod) {
            String[] parts = classNameWithMethod.split("#");
            String className = parts[0];
            String methodName = parts[1];
            mWhitelistEntries.add(
                    violation -> {
                        if ((violation.violationType() & violationType) == 0) {
                            return null;
                        }
                        for (StackTraceElement frame : violation.stackTrace()) {
                            if (frame.getClassName().equals(className)
                                    && frame.getMethodName().equals(methodName)) {
                                return violationType;
                            }
                        }
                        return null;
                    });
            return this;
        }

        /**
         * Ignore a violation that occurs inside your app.
         *
         * @param violationType A mask containing one or more of the DETECT_* constants.
         * @param classNameWithMethod The name of the class and method to ignore StrictMode
         *         violations
         *     in. The format must be "package.Class#method", for example,
         *     "com.google.foo.StrictModeWhitelist#addAllowedMethod".
         */
        public Builder addAllowedMethod(int violationType, final String classNameWithMethod) {
            return ignoreExternalMethod(violationType, classNameWithMethod);
        }

        /** Set the custom penalty that will be notified when an unwhitelisted violation occurs. */
        public Builder setCustomPenalty(Consumer<Violation> penalty) {
            mCustomPenalty = penalty;
            return this;
        }

        /**
         * Replaces all penalties with the death penalty.
         *
         * <p>Installing whitelists requires that StrictMode does not have the death penalty. If
         * your app requires the death penalty, you can set this, which will attempt to emulate the
         * system behavior if possible.
         *
         * <p>Death is not guaranteed, since it relies on reflection to work.
         */
        public Builder replaceAllPenaltiesWithDeathPenalty() {
            mCustomPenalty =
                    info -> {
                        StrictModePolicyViolation toThrow = new StrictModePolicyViolation(info);
                        // Post task so that no one has a chance to catch the thrown exception.
                        new Handler(Looper.getMainLooper())
                                .post(
                                        () -> {
                                            throw toThrow;
                                        });
                    };
            return this;
        }

        /** Make immutable. */
        public ThreadStrictModeInterceptor build() {
            return new ReflectiveThreadStrictModeInterceptor(mWhitelistEntries, mCustomPenalty);
        }
    }
}
