// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.origin_matcher;

import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Arrays;
import java.util.List;

/**
 * A thread-safe thin Java facing implementation of `origin_matcher.h`. This API sends all the data and
 * origin matching requests over to the native implementation so that we can have a standard
 * implementation for our origin matching rules.
 *
 * <p>This class owns a native object so it is critical that when you are done using this, it needs
 * to be cleaned up with the {@link OriginMatcher#destroy} method.
 */
@NullMarked
@JNINamespace("origin_matcher")
public class OriginMatcher {
    private final Object mLock = new Object();

    private long mNative;

    public OriginMatcher() {
        mNative = OriginMatcherJni.get().create();
    }

    /** Returns if any of the rules in the origin matcher currently match the uri provided. */
    public boolean matchesOrigin(Uri origin) {
        return matchesOriginLocked(origin);
    }

    /**
     * Updates the rules to the list provided. If any badly formed rules are provided, the rules
     * will not be updated and the bad rules will be returned.
     */
    public List<String> setRuleList(final List<String> rules) {
        return Arrays.asList(setRuleListLocked(rules));
    }

    /**
     * Returns an array of string rules that represent this OriginMatcher. Preference storing this
     * in Java when you need to persist the lifetime of origin matcher rules as it is easier to
     * reason about than the native counter parts clean up.
     */
    public List<String> serialize() {
        return Arrays.asList(serializeLocked());
    }

    /**
     * This method _must_ be called when you are finished with the origin matcher.
     *
     * <p>It will clean up any native references of your code.
     */
    public void destroy() {
        destroyLocked();
    }

    /**
     * Called by native to make a copy of OriginMatcher when passing a reference to the native
     * counter part.
     */
    @CalledByNative
    private long getNative() {
        return mNative;
    }

    private void ensureNativeExists() {
        if (mNative == 0) {
            throw new IllegalStateException(
                    "OriginMatcher did not have access to native implementation. "
                            + "Ensure you don't call this method after cleanup.");
        }
    }

    private boolean matchesOriginLocked(Uri origin) {
        synchronized (mLock) {
            ensureNativeExists();
            return OriginMatcherJni.get()
                    .matches(mNative, Origin.create(new GURL(origin.toString())));
        }
    }

    private String[] setRuleListLocked(final List<String> rules) {
        synchronized (mLock) {
            ensureNativeExists();
            return OriginMatcherJni.get().setRuleList(mNative, rules.toArray(new String[] {}));
        }
    }

    private String[] serializeLocked() {
        synchronized (mLock) {
            ensureNativeExists();
            return OriginMatcherJni.get().serialize(mNative);
        }
    }

    private void destroyLocked() {
        synchronized (mLock) {
            ensureNativeExists();
            OriginMatcherJni.get().destroy(mNative);
            mNative = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long create();

        @JniType("bool")
        boolean matches(long ptr, @JniType("url::Origin") Origin origin);

        // Returns the list of invalid rules.
        // If there are bad rules, no update is performed
        @JniType("std::vector<std::string>")
        String[] setRuleList(long ptr, @JniType("std::vector<std::string>") String[] rules);

        @JniType("std::vector<std::string>")
        String[] serialize(long ptr);

        void destroy(long ptr);
    }
}
