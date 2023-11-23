// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

/** Class for testing for failed child frame navigations for external intents. */
@JNINamespace("external_intents")
public class TestChildFrameNavigationObserver {
    private final CallbackHelper mFailCallback;
    private final CallbackHelper mFinishCallback;
    private final CallbackHelper mLoadCallback;
    private final WebContents mWebContents;

    public TestChildFrameNavigationObserver(
            WebContents webContents,
            CallbackHelper failCallback,
            CallbackHelper finishCallback,
            CallbackHelper loadCallback) {
        mWebContents = webContents;
        mFailCallback = failCallback;
        mFinishCallback = finishCallback;
        mLoadCallback = loadCallback;
    }

    public static TestChildFrameNavigationObserver createAndAttachToNativeWebContents(
            WebContents webContents,
            CallbackHelper failCallback,
            CallbackHelper finishCallback,
            CallbackHelper loadCallback) {
        ThreadUtils.assertOnUiThread();

        TestChildFrameNavigationObserver newObserver =
                new TestChildFrameNavigationObserver(
                        webContents, failCallback, finishCallback, loadCallback);
        TestChildFrameNavigationObserverJni.get()
                .createAndAttachToNativeWebContents(newObserver, webContents);
        return newObserver;
    }

    @CalledByNative
    public void didStartNavigation(NavigationHandle navigation) {
        mLoadCallback.notifyCalled();
    }

    @CalledByNative
    public void didFinishNavigation(NavigationHandle navigation) {
        if (navigation.errorCode() == 0) {
            mFinishCallback.notifyCalled();
        } else {
            mFailCallback.notifyCalled();
        }
    }

    @NativeMethods
    public interface Natives {
        void createAndAttachToNativeWebContents(
                TestChildFrameNavigationObserver caller, WebContents webContents);
    }
}
