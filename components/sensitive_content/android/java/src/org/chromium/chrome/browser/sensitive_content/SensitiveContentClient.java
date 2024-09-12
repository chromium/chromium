// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Java counterpart of the `AndroidSensitiveContentClient`. Used to retrieve the container view and
 * set its content sensitivity.
 */
@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
class SensitiveContentClient implements ViewAndroidDelegate.ContainerViewObserver {
    /** Used to update the content sensitivity of the current container view. */
    @VisibleForTesting
    static interface ContentSensitivitySetter {
        /**
         * Updates the content sensitivity of the given {@code containerView} to {@code
         * contentIsSensitive}.
         *
         * @param containerView The container view on which the content sensitivity is set.
         * @param contentIsSensitive The new content sensitivity.
         */
        void setContentSensitivity(View containerView, boolean contentIsSensitive);
    }

    /** Caches the current content sensitivity of the {@link WebContents}. */
    private boolean mContentIsSensitive;

    /** {@link WebContents} showing the current page. */
    private final WebContents mWebContents;

    /**
     * The {@link ViewAndroidDelegate} this class is observing. The {@link ViewAndroidDelegate} of
     * the {@link WebContents} can be swapped, so the code needs to stop observing the last one and
     * start observing the new one.
     */
    private @Nullable ViewAndroidDelegate mLastViewAndroidDelegate;

    /**
     * Sets the provided content sensitivity on the provided view, in production. In tests, it is
     * overridden by a mock. Mocking the {@link View} directly is not possible yet, because JUnit
     * does not yet support Android V's sensitive content API.
     */
    private final ContentSensitivitySetter mContentSensitivitySetter;

    @CalledByNative
    private SensitiveContentClient(WebContents webContents) {
        this(
                webContents,
                (containerView, contentIsSensitive) -> {
                    containerView.setContentSensitivity(
                            contentIsSensitive
                                    ? View.CONTENT_SENSITIVITY_SENSITIVE
                                    : View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
                });
    }

    @VisibleForTesting
    SensitiveContentClient(
            WebContents webContents, ContentSensitivitySetter contentSensitivitySetter) {
        mWebContents = webContents;
        mLastViewAndroidDelegate = mWebContents.getViewAndroidDelegate();
        if (mLastViewAndroidDelegate != null) {
            mLastViewAndroidDelegate.addObserver(this);
        }
        mContentSensitivitySetter = contentSensitivitySetter;
    }

    @CalledByNative
    private void destroy() {
        if (mLastViewAndroidDelegate != null) {
            mLastViewAndroidDelegate.removeObserver(this);
        }
    }

    /**
     * The container view of the {@link WebContents} can be swapped. This method sets the current
     * content sensitivity on the new container view.
     *
     * @param view The new container view.
     */
    @Override
    public void onUpdateContainerView(ViewGroup view) {
        assert view == mWebContents.getViewAndroidDelegate().getContainerView();
        setContentSensitivity(mContentIsSensitive);
    }

    /**
     * Updates the content sensitivity of the container view. Called by native when the content
     * sensitivity changes.
     *
     * @param contentIsSensitive Content sensitivity.
     */
    @CalledByNative
    @VisibleForTesting
    void setContentSensitivity(boolean contentIsSensitive) {
        mContentIsSensitive = contentIsSensitive;

        ViewAndroidDelegate viewAndroidDelegate = mWebContents.getViewAndroidDelegate();
        if (mLastViewAndroidDelegate != viewAndroidDelegate) {
            if (mLastViewAndroidDelegate != null) {
                mLastViewAndroidDelegate.removeObserver(this);
            }
            if (viewAndroidDelegate != null) {
                viewAndroidDelegate.addObserver(this);
            }
            mLastViewAndroidDelegate = viewAndroidDelegate;
        }

        if (viewAndroidDelegate == null) {
            return;
        }
        View containerView = viewAndroidDelegate.getContainerView();
        if (containerView == null) {
            return;
        }

        mContentSensitivitySetter.setContentSensitivity(containerView, mContentIsSensitive);
    }
}
