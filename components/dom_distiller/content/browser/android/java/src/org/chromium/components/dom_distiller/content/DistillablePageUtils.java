// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.content;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** Provides access to the native dom_distiller::IsPageDistillable function. */
@JNINamespace("dom_distiller::android")
@NullMarked
public final class DistillablePageUtils {
    // A map of native observer objects to their Java counterparts allows unlimited scaling in
    // number of web contents. Another java object owns the PageDistillableDelegate objects.
    private static final Map<Long, WeakReference<PageDistillableDelegate>> sNativeHelperMap =
            new HashMap<>();

    /** Delegate to receive distillability updates. */
    public interface PageDistillableDelegate {
        /**
         * Called when the distillability status changes.
         *
         * @param url The url for the result.
         * @param isDistillable Whether the page is distillable.
         * @param isLast Whether the update is the last one for this page.
         * @param isLongArticle Whether the page is a long article.
         * @param isMobileOptimized Whether the page is optimized for mobile. Only valid when the
         *     heuristics is ADABOOST_MODEL or ALL_ARTICLES.
         */
        void onIsPageDistillableResult(
                GURL url,
                boolean isDistillable,
                boolean isLast,
                boolean isLongArticle,
                boolean isMobileOptimized);
    }

    /**
     * Sets the delegate to receive distillability updates.
     *
     * @param webContents The web contents to set the delegate for.
     * @param delegate The delegate to set. It will only be held by a weak reference so it is
     *     assumed that the delegate will be owned by another object in Java.
     */
    public static void setDelegate(
            WebContents webContents, @Nullable PageDistillableDelegate delegate) {
        long nativeObserverPtr = DistillablePageUtilsJni.get().setDelegate(webContents, delegate);
        if (nativeObserverPtr == 0) {
            return;
        }
        sNativeHelperMap.put(nativeObserverPtr, new WeakReference<>(delegate));
    }

    private static @Nullable PageDistillableDelegate getDelegate(long nativeObserverPtr) {
        WeakReference<PageDistillableDelegate> delegate = sNativeHelperMap.get(nativeObserverPtr);
        return delegate == null ? null : delegate.get();
    }

    @CalledByNative
    private static void onNativeDestroyed(long nativeObserverPtr) {
        sNativeHelperMap.remove(nativeObserverPtr);
    }

    @CalledByNative
    private static void callOnIsPageDistillableUpdate(
            long nativeObserverPtr,
            @JniType("GURL") GURL url,
            boolean isDistillable,
            boolean isLast,
            boolean isLongArticle,
            boolean isMobileOptimized) {
        PageDistillableDelegate delegate = getDelegate(nativeObserverPtr);
        if (delegate != null) {
            delegate.onIsPageDistillableResult(
                    url, isDistillable, isLast, isLongArticle, isMobileOptimized);
        }
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        long setDelegate(
                @JniType("content::WebContents*") WebContents webContents,
                @Nullable PageDistillableDelegate delegate);
    }
}
