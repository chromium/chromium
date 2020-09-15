// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.bridges;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;

import java.util.ArrayList;
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
        callback.onResult(getSampleData());
    }

    private List<Tutorial> getSampleData() {
        List<Tutorial> list = new ArrayList<>();
        list.add(new Tutorial(FeatureType.DOWNLOAD,
                "How to use Google Chrome's download functionality",
                "https://storage.googleapis.com/stock-wizard.appspot.com/portrait.jpg",
                "https://storage.googleapis.com/stock-wizard.appspot.com/portrait.jpg",
                "caption url", "share url", 35));

        list.add(new Tutorial(FeatureType.SEARCH, "How to efficiently search with Google Chrome",
                "https://storage.googleapis.com/stock-wizard.appspot.com/elephant.jpg ",
                "https://storage.googleapis.com/stock-wizard.appspot.com/elephant.jpg",
                "caption url", "share url", 35));
        return list;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeVideoTutorialServiceBridge = 0;
    }
}
