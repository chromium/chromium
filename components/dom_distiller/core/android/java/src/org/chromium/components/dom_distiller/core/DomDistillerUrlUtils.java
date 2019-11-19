// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Wrapper for the dom_distiller::url_utils.
 */
@JNINamespace("dom_distiller::url_utils::android")
public final class DomDistillerUrlUtils {
    private DomDistillerUrlUtils() {
    }

    /**
     * Returns the URL for viewing distilled content for a URL.
     *
     * @param scheme The scheme for the DOM Distiller source.
     * @param url The URL to distill.
     * @return the URL to load to get the distilled version of a page.
     */
    @VisibleForTesting
    public static String getDistillerViewUrlFromUrl(String scheme, String url) {
        assert scheme != null;
        if (TextUtils.isEmpty(url)) return url;
        return DomDistillerUrlUtilsJni.get().getDistillerViewUrlFromUrl(scheme, url);
    }

    /**
     * Returns the original URL of a distillation given the viewer URL.
     *
     * @param url The current viewer URL.
     * @return the URL of the original page.
     */
    public static String getOriginalUrlFromDistillerUrl(String url) {
        if (TextUtils.isEmpty(url)) return url;
        return DomDistillerUrlUtilsJni.get().getOriginalUrlFromDistillerUrl(url);
    }

    /**
     * Returns whether the url is for a distilled page.
     *
     * @param url The url of the page.
     * @return whether the url is for a distilled page.
     */
    public static boolean isDistilledPage(String url) {
        if (TextUtils.isEmpty(url)) return false;
        return DomDistillerUrlUtilsJni.get().isDistilledPage(url);
    }

    public static String getValueForKeyInUrl(String url, String key) {
        assert key != null;
        if (TextUtils.isEmpty(url)) return null;
        return DomDistillerUrlUtilsJni.get().getValueForKeyInUrl(url, key);
    }

    @NativeMethods
    interface Natives {
        String getDistillerViewUrlFromUrl(String scheme, String url);
        String getOriginalUrlFromDistillerUrl(String viewerUrl);
        boolean isDistilledPage(String url);
        String getValueForKeyInUrl(String url, String key);
    }
}
