// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Collection of utility methods that operates on Tab for Fusebox. */
@NullMarked
public class FuseboxTabUtils {
    /**
     * Returns the whether a tab is eligible for attaching it's web content. This does not exclude
     * tabs based on specific tab model - including incognito tab model.
     *
     * @param tab The tab to be checked.
     */
    public static boolean isTabEligibleForAttachment(@Nullable Tab tab) {
        // TODO: This also has to check the eligibility here:
        // components/optimization_guide/content/browser/page_context_eligibility.h
        return tab != null
                && (tab.getUrl().getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || tab.getUrl().getScheme().equals(UrlConstants.HTTPS_SCHEME));
    }

    /**
     * Returns the whether a tab is active.
     *
     * @param tab The tab to be checked.
     */
    public static boolean isTabActive(@Nullable Tab tab) {
        return tab != null
                && tab.isInitialized()
                && !tab.isFrozen()
                && tab.getWebContents() != null
                && !tab.getWebContents().isLoading()
                && tab.getWebContents().getRenderWidgetHostView() != null;
    }

    /**
     * Returns the drawable given the favicon of the tab.
     *
     * @param context An Android context.
     * @param favicon The favicon of the tab.
     * @param iconSizePx The size (both width and height) to scale to.
     */
    public static Drawable getDrawableForTabFavicon(
            Context context, @Nullable Bitmap favicon, @Px int iconSizePx) {
        Drawable drawable;
        if (favicon != null) {
            Bitmap bitmap =
                    Bitmap.createScaledBitmap(favicon, iconSizePx, iconSizePx, /* filter= */ true);
            drawable = new BitmapDrawable(context.getResources(), bitmap);
            drawable.setBounds(
                    /* left= */ 0, /* top= */ 0, /* right= */ iconSizePx, /* bottom= */ iconSizePx);
        } else {
            drawable = assumeNonNull(context.getDrawable(R.drawable.ic_globe_24dp));
        }
        return drawable;
    }
}
