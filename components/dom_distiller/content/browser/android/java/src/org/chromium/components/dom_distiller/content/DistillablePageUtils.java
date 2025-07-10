// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.content;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Provides access to the native dom_distiller::IsPageDistillable function. */
@JNINamespace("dom_distiller::android")
@NullMarked
public final class DistillablePageUtils {
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

    public static void setDelegate(WebContents webContents, PageDistillableDelegate delegate) {
        DistillablePageUtilsJni.get().setDelegate(webContents, delegate);
    }

    @CalledByNative
    private static void callOnIsPageDistillableUpdate(
            PageDistillableDelegate delegate,
            GURL url,
            boolean isDistillable,
            boolean isLast,
            boolean isLongArticle,
            boolean isMobileOptimized) {
        if (delegate != null) {
            delegate.onIsPageDistillableResult(
                    url, isDistillable, isLast, isLongArticle, isMobileOptimized);
        }
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void setDelegate(WebContents webContents, PageDistillableDelegate delegate);
    }
}
