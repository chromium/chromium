// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

/**
 * Class for testing for failed child frame navigations for external intents.
 */
@JNINamespace("external_intents")
public class TestChildFrameNavigationObserver {
    private final CallbackHelper mFailCallback;
    private final WebContents mWebContents;

    public TestChildFrameNavigationObserver(
            WebContents webContents, final CallbackHelper failCallback) {
        mWebContents = webContents;
        mFailCallback = failCallback;
    }

    public static TestChildFrameNavigationObserver createAndAttachToNativeWebContents(
            WebContents webContents, CallbackHelper failCallback) {
        ThreadUtils.assertOnUiThread();

        TestChildFrameNavigationObserver newObserver =
                new TestChildFrameNavigationObserver(webContents, failCallback);
        TestChildFrameNavigationObserverJni.get().createAndAttachToNativeWebContents(
                newObserver, webContents);
        return newObserver;
    }

    @CalledByNative
    public void didFinishNavigation(NavigationHandle navigation) {
        if (navigation.errorCode() == 0) return;
        mFailCallback.notifyCalled();
    }

    @NativeMethods
    public interface Natives {
        void createAndAttachToNativeWebContents(
                TestChildFrameNavigationObserver caller, WebContents webContents);
    }
}
