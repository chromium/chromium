// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.JniOnceRunnable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.base.WindowAndroid;

/**
 * Lets native browser tests wait for layout transitions, as {@code LayoutTestUtils} does for Java
 * tests. Backs the native {@code android_ui_test_utils::WaitForBrowsingLayout()}.
 */
@JNINamespace("android_ui_test_utils")
@NullMarked
public class LayoutTestBridge implements LayoutStateObserver {
    private final LayoutManager mLayoutManager;
    private final JniOnceRunnable mCallback;

    private LayoutTestBridge(LayoutManager layoutManager, JniOnceRunnable callback) {
        mLayoutManager = layoutManager;
        mCallback = callback;
    }

    /**
     * Runs {@code callback} once the browsing layout of {@code window} is showing and is not in a
     * transition. Runs it synchronously if that is already the case.
     *
     * @return The wait. The caller must call {@link #destroy()} on it when it stops waiting.
     */
    @CalledByNative
    private static LayoutTestBridge waitForBrowsingLayout(
            @JniType("ui::WindowAndroid*") WindowAndroid window,
            @JniType("base::OnceClosure&&") JniOnceRunnable callback) {
        LayoutManager layoutManager = LayoutManagerProvider.from(window);
        assert layoutManager != null;
        LayoutTestBridge bridge = new LayoutTestBridge(layoutManager, callback);
        if (bridge.isBrowsingLayoutSettled()) {
            callback.run();
        } else {
            layoutManager.addObserver(bridge);
        }
        return bridge;
    }

    /** Stops observing, and frees the callback if it never ran, e.g. when the wait timed out. */
    @CalledByNative
    private void destroy() {
        mLayoutManager.removeObserver(this);
        mCallback.destroy();
    }

    @Override
    public void onFinishedShowing(@LayoutType int layoutType) {
        if (!isBrowsingLayoutSettled()) return;
        mLayoutManager.removeObserver(this);
        mCallback.run();
    }

    private boolean isBrowsingLayoutSettled() {
        return mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING
                && !mLayoutManager.isLayoutStartingToShow(LayoutType.BROWSING)
                && !mLayoutManager.isLayoutStartingToHide(LayoutType.BROWSING);
    }
}
