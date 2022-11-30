// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.bridges;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;

import java.util.Arrays;
import java.util.List;

/**
 * Bridge to the native video tutorial service for the given {@link Profile}.
 */
@JNINamespace("video_tutorials")
public class VideoTutorialServiceBridge implements VideoTutorialService {
    private long mNativeVideoTutorialServiceBridge;

    @CalledByNative
    private static VideoTutorialServiceBridge create(long nativePtr) {
        return new VideoTutorialServiceBridge(nativePtr);
    }

    private VideoTutorialServiceBridge(long nativePtr) {
        mNativeVideoTutorialServiceBridge = nativePtr;
    }

    @Override
    public void getTutorials(Callback<List<Tutorial>> callback) {
        if (mNativeVideoTutorialServiceBridge == 0) return;
        VideoTutorialServiceBridgeJni.get().getTutorials(
                mNativeVideoTutorialServiceBridge, this, callback);
    }

    @Override
    public void getTutorial(@FeatureType int feature, Callback<Tutorial> callback) {
        if (mNativeVideoTutorialServiceBridge == 0) return;
        VideoTutorialServiceBridgeJni.get().getTutorial(
                mNativeVideoTutorialServiceBridge, this, feature, callback);
    }

    @Override
    public List<String> getSupportedLanguages() {
        if (mNativeVideoTutorialServiceBridge == 0) return null;
        String[] languages = VideoTutorialServiceBridgeJni.get().getSupportedLanguages(
                mNativeVideoTutorialServiceBridge, this);
        return Arrays.asList(languages);
    }

    @Override
    public List<String> getAvailableLanguagesForTutorial(@FeatureType int feature) {
        if (mNativeVideoTutorialServiceBridge == 0) return null;
        String[] languages = VideoTutorialServiceBridgeJni.get().getAvailableLanguagesForTutorial(
                mNativeVideoTutorialServiceBridge, this, feature);
        return Arrays.asList(languages);
    }

    @Override
    public String getPreferredLocale() {
        if (mNativeVideoTutorialServiceBridge == 0) return null;
        return VideoTutorialServiceBridgeJni.get().getPreferredLocale(
                mNativeVideoTutorialServiceBridge, this);
    }

    @Override
    public void setPreferredLocale(String locale) {
        if (mNativeVideoTutorialServiceBridge == 0) return;
        VideoTutorialServiceBridgeJni.get().setPreferredLocale(
                mNativeVideoTutorialServiceBridge, this, locale);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeVideoTutorialServiceBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void getTutorials(long nativeVideoTutorialServiceBridge, VideoTutorialServiceBridge caller,
                Callback<List<Tutorial>> callback);
        void getTutorial(long nativeVideoTutorialServiceBridge, VideoTutorialServiceBridge caller,
                int feature, Callback<Tutorial> callback);
        String[] getSupportedLanguages(
                long nativeVideoTutorialServiceBridge, VideoTutorialServiceBridge caller);
        String[] getAvailableLanguagesForTutorial(long nativeVideoTutorialServiceBridge,
                VideoTutorialServiceBridge caller, int feature);
        String getPreferredLocale(
                long nativeVideoTutorialServiceBridge, VideoTutorialServiceBridge caller);
        void setPreferredLocale(long nativeVideoTutorialServiceBridge,
                VideoTutorialServiceBridge caller, String locale);
    }
}
