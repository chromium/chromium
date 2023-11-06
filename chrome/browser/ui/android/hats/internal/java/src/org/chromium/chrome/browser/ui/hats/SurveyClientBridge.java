// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Glue code that's used to pass C++ initiated SurveyClient to Java. Thus, this class should not be
 * initialized from Java.
 */
@JNINamespace("hats")
class SurveyClientBridge implements SurveyClient {
    private final SurveyClient mDelegate;
    private long mNativeSurveyClient;

    private SurveyClientBridge(long nativeSurveyClient, SurveyClient delegate) {
        mNativeSurveyClient = nativeSurveyClient;
        mDelegate = delegate;
    }

    @CalledByNative
    @VisibleForTesting
    static SurveyClientBridge create(
            long nativeSurveyClient, String trigger, SurveyUiDelegate uiDelegate, Profile profile) {
        assert SurveyClientFactory.getInstance() != null;
        SurveyConfig config = SurveyConfig.get(trigger);
        if (config == null) return null;

        return new SurveyClientBridge(
                nativeSurveyClient,
                SurveyClientFactory.getInstance().createClient(config, uiDelegate, profile));
    }

    /**
     * Called from Java to show a survey without PSD. Used if SurveyUiDelegate is created from C++.
     */
    @Override
    public void showSurvey(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mDelegate.showSurvey(activity, lifecycleDispatcher);
    }

    /**
     * Called from Java to show a survey with PSD. Used if SurveyUiDelegate is created from C++.
     */
    @Override
    public void showSurvey(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher,
            Map<String, Boolean> surveyPsdBitValues, Map<String, String> surveyPsdStringValues) {
        mDelegate.showSurvey(
                activity, lifecycleDispatcher, surveyPsdBitValues, surveyPsdStringValues);
    }

    /**
     * Called when a C++ client wants to display a survey with PSD.
     */
    @CalledByNative
    void showSurvey(WindowAndroid windowAndroid, String[] surveyPsdBitFields,
            boolean[] surveyPsdBitValues, String[] surveyPsdStringFields,
            String[] surveyPsdStringValues) {
        assert surveyPsdBitFields.length == surveyPsdBitValues.length;
        assert surveyPsdStringFields.length == surveyPsdStringValues.length;

        Map<String, Boolean> bitsValues = new HashMap<>();
        for (int i = 0; i < surveyPsdBitFields.length; ++i) {
            bitsValues.put(surveyPsdBitFields[i], surveyPsdBitValues[i]);
        }
        Map<String, String> stringValues = new HashMap<>();
        for (int i = 0; i < surveyPsdStringFields.length; ++i) {
            stringValues.put(surveyPsdStringFields[i], surveyPsdStringValues[i]);
        }

        Activity activity = windowAndroid.getActivity().get();
        showSurvey(activity, null, bitsValues, stringValues);
    }
}
