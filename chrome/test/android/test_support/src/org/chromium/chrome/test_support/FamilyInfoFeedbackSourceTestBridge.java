// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.feedback.FamilyInfoFeedbackSource;
import org.chromium.chrome.browser.feedback.FeedbackSource;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;

/** Test support for injecting test behavior from C++ tests to Android Feedback sources. */
@JNINamespace("chrome::android")
public class FamilyInfoFeedbackSourceTestBridge {
    /** Returns the value associated with the given key in the FeedbackSource mapping. */
    @CalledByNative
    public static String getValue(FeedbackSource source, String key) {
        Map<String, String> feedbackMap = source.getFeedback();
        return feedbackMap.containsKey(key) ? feedbackMap.get(key) : "";
    }

    /** Returns a FamilyInfoFeedbackSource Java object for testing. */
    @CalledByNative
    public static FamilyInfoFeedbackSource createFamilyInfoFeedbackSource(Profile profile) {
        return new FamilyInfoFeedbackSource(profile);
    }
}
