// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** Wrapper for the dom_distiller::url_utils. */
@JNINamespace("dom_distiller::url_utils::android")
@NullMarked
public final class DomDistillerUrlUtils {
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

    /**
     * Returns whether the url is for a distilled page.
     *
     * @param urlSpec The url spec of the page See {GURL#getSpec}.
     * @return whether the url is for a distilled page.
     */
    public static boolean isDistilledPage(@Nullable String urlSpec) {
        if (TextUtils.isEmpty(urlSpec)) return false;
        if (!urlSpec.startsWith(UrlConstants.DISTILLER_SCHEME + ":")) return false;
        return DomDistillerUrlUtilsJni.get().isDistilledPage(urlSpec);
    }

    /**
     * Returns whether the url is for a distilled page.
     *
     * @param url The url of the page.
     * @return whether the url is for a distilled page.
     */
    public static boolean isDistilledPage(@Nullable GURL url) {
        if (url == null) return false;
        if (!url.getScheme().equals(UrlConstants.DISTILLER_SCHEME)) return false;
        return isDistilledPage(url.getSpec());
    }

    public static @Nullable String getValueForKeyInUrl(String url, String key) {
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
