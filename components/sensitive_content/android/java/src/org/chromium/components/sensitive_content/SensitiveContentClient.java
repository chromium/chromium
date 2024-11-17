// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sensitive_content;

import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.lang.ref.WeakReference;

/**
 * Java counterpart of the `AndroidSensitiveContentClient`. Used to retrieve the container view and
 * set its content sensitivity.
 */
@JNINamespace("sensitive_content")
public class SensitiveContentClient implements ViewAndroidDelegate.ContainerViewObserver {
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
     *
     * <p>It can happen that the {@link SensitiveContentClient} is the last object holding a
     * reference to the {@link ViewAndroidDelegate}. Therefore, the reference is weak, to allow
     * garbage collection.
     */
    private WeakReference<ViewAndroidDelegate> mLastViewAndroidDelegate;

    /**
     * Sets the provided content sensitivity on the provided view, in production. In tests, it is
     * overridden by a mock. Mocking the {@link View} directly is not possible yet, because JUnit
     * does not yet support Android V's sensitive content API.
     */
    private final ContentSensitivitySetter mContentSensitivitySetter;

    private final ObserverList<Observer> mObservers;

    /**
     * Retrieves the client from {@link WebContents}, by calling the native client. The native
     * client owns the java client, and the native client has its lifetime tied to the {@link
     * WebContents}.
     */
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static SensitiveContentClient fromWebContents(WebContents webContents) {
        return SensitiveContentClientJni.get()
                .getJavaSensitiveContentClientFromWebContents(webContents);
    }

    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
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
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    SensitiveContentClient(
            WebContents webContents, ContentSensitivitySetter contentSensitivitySetter) {
        mWebContents = webContents;
        mLastViewAndroidDelegate =
                new WeakReference<ViewAndroidDelegate>(mWebContents.getViewAndroidDelegate());
        if (mLastViewAndroidDelegate.get() != null) {
            mLastViewAndroidDelegate.get().addObserver(this);
        }
        mContentSensitivitySetter = contentSensitivitySetter;
        mObservers = new ObserverList<Observer>();
    }

    @CalledByNative
    private void destroy() {
        if (mLastViewAndroidDelegate.get() != null) {
            mLastViewAndroidDelegate.get().removeObserver(this);
        }
        mObservers.clear();
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
        final ViewAndroidDelegate viewAndroidDelegate = mWebContents.getViewAndroidDelegate();
        if (mLastViewAndroidDelegate.get() != viewAndroidDelegate) {
            if (mLastViewAndroidDelegate.get() != null) {
                mLastViewAndroidDelegate.get().removeObserver(this);
            }
            if (viewAndroidDelegate != null) {
                viewAndroidDelegate.addObserver(this);
            }
            mLastViewAndroidDelegate = new WeakReference<ViewAndroidDelegate>(viewAndroidDelegate);
        }

        if (viewAndroidDelegate == null) {
            return;
        }
        View containerView = viewAndroidDelegate.getContainerView();
        if (containerView == null) {
            return;
        }

        mContentSensitivitySetter.setContentSensitivity(containerView, contentIsSensitive);
        if (mContentIsSensitive != contentIsSensitive) {
            mContentIsSensitive = contentIsSensitive;
            notifyObserversAboutSensitivityChange(contentIsSensitive);
        }
    }

    /** Observes changes made by the {@link SensitiveContentClient}. */
    public static interface Observer {
        /**
         * Called when the content sensitivity changed.
         *
         * @param contentIsSensitive True if the content is sensitive.
         */
        void onContentSensitivityChanged(boolean contentIsSensitive);
    }

    /**
     * Add an observer to the list of observers.
     *
     * @param observer The {@link Observer} instance to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer from the list of observers.
     *
     * @param observer The {@link Observer} instance to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies the observers about the sensitivity change.
     *
     * @param contentIsSensitive True if the content is sensitive.
     */
    private void notifyObserversAboutSensitivityChange(boolean contentIsSensitive) {
        for (final Observer observer : mObservers) {
            observer.onContentSensitivityChanged(contentIsSensitive);
        }
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        SensitiveContentClient getJavaSensitiveContentClientFromWebContents(
                WebContents webContents);
    }
}
