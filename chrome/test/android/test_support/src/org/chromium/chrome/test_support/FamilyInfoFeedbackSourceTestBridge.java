// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.chrome.browser.feedback.FamilyInfoFeedbackSource;
import org.chromium.chrome.browser.feedback.FeedbackSource;
import org.chromium.chrome.browser.profiles.Profile;

/** Test support for injecting test behavior from C++ tests to Android Feedback sources. */
@JNINamespace("chrome::android")
public class FamilyInfoFeedbackSourceTestBridge {
    /** Returns the value associated with the given key in the FeedbackSource mapping. */
    @CalledByNative
    public static @JniType("std::string") String getValue(
            FeedbackSource source, @JniType("std::string") String key) {
        return source.getFeedback().getOrDefault(key, "");
    }

    /** Returns a FamilyInfoFeedbackSource Java object for testing. */
    @CalledByNative
    public static FamilyInfoFeedbackSource createFamilyInfoFeedbackSource(
            @JniType("Profile*") Profile profile) {
        return new FamilyInfoFeedbackSource(profile);
    }
}
