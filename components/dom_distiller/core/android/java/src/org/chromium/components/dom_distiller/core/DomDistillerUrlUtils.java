// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

/** Wrapper for the dom_distiller::url_utils. */
@JNINamespace("dom_distiller::url_utils::android")
public final class DomDistillerUrlUtils {
    // Keep in sync with components/dom_distiller/core/url_constants.cc
    private static final String DOM_DISTILLER_SCHEME = "chrome-distiller";

    private DomDistillerUrlUtils() {}

    /**
     * Returns the URL for viewing distilled content for a URL.
     *
     * @param scheme The scheme for the DOM Distiller source.
     * @param url The URL to distill.
     * @param title The title of the page being distilled.
     * @return the URL to load to get the distilled version of a page.
     */
    @VisibleForTesting
    public static String getDistillerViewUrlFromUrl(String scheme, String url, String title) {
        assert scheme != null;
        if (TextUtils.isEmpty(url)) return url;
        return DomDistillerUrlUtilsJni.get().getDistillerViewUrlFromUrl(scheme, url, title);
    }

    @Deprecated
    public static String getOriginalUrlFromDistillerUrl(String url) {
        if (TextUtils.isEmpty(url)) return url;
        return DomDistillerUrlUtilsJni.get().getOriginalUrlFromDistillerUrl(url).getSpec();
    }

    /**
     * Returns the original URL of a distillation given the viewer URL.
     *
     * @param url The current viewer URL.
     * @return the URL of the original page.
     */
    public static GURL getOriginalUrlFromDistillerUrl(GURL url) {
        if (url.isEmpty()) return url;
        return DomDistillerUrlUtilsJni.get().getOriginalUrlFromDistillerUrl(url.getSpec());
    }

    public static boolean isDistilledPage(String url) {
        if (TextUtils.isEmpty(url)) return false;
        if (!url.startsWith(DOM_DISTILLER_SCHEME + ":")) return false;
        return DomDistillerUrlUtilsJni.get().isDistilledPage(url);
    }

    /**
     * Returns whether the url is for a distilled page.
     *
     * @param url The url of the page.
     * @return whether the url is for a distilled page.
     */
    public static boolean isDistilledPage(GURL url) {
        if (!url.getScheme().equals(DOM_DISTILLER_SCHEME)) return false;
        return isDistilledPage(url.getSpec());
    }

    public static String getValueForKeyInUrl(String url, String key) {
        assert key != null;
        if (TextUtils.isEmpty(url)) return null;
        return DomDistillerUrlUtilsJni.get().getValueForKeyInUrl(url, key);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        String getDistillerViewUrlFromUrl(String scheme, String url, String title);

        GURL getOriginalUrlFromDistillerUrl(String viewerUrl);

        boolean isDistilledPage(String url);

        String getValueForKeyInUrl(String url, String key);
    }
}
