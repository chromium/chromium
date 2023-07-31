// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/**
 * Favicon fetching mechanism with multi-step fallback functionality.
 * The class supports very simple caching mechanism which remembers what type of icon is available
 * for a given URL to reduce number of JNI and object creation calls.
 */
public class FaviconFetcher {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    private int mDesiredFaviconWidthPx;
    private @NonNull RoundedIconGenerator mIconGenerator;
    private @Nullable LargeIconBridge mIconBridge;

    /**
     * Constructor.
     *
     * @param context An Android context.
     */
    public FaviconFetcher(@NonNull Context context) {
        mDesiredFaviconWidthPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);

        int fallbackIconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int fallbackIconColor = context.getColor(R.color.default_favicon_background_color);
        int fallbackIconTextSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator = new RoundedIconGenerator(fallbackIconSize, fallbackIconSize,
                fallbackIconSize / 2, fallbackIconColor, fallbackIconTextSize);
    }

    /**
     * Release any resources and deregister any callbacks created by this class.
     */
    public void destroy() {
        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }
    }

    /**
     * Notify that the current User profile has changed.
     *
     * @param profile Current user profile.
     */
    public void setProfile(Profile profile) {
        if (mIconBridge != null) {
            mIconBridge.destroy();
        }

        mIconBridge = new LargeIconBridge(profile);
        resetCache();
    }

    /**
     * Asynchronously retrieve favicon for a given url and deliver the result via supplied callback.
     * All fetches are done asynchronously, but the caller should expect a synchronous call in some
     * cases, eg if an icon cannot be retrieved because a corresponding provider is not available.
     *
     * @param url The url to retrieve a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void fetchFavicon(@NonNull GURL url, @NonNull Callback<Bitmap> callback) {
        if (mIconBridge == null) {
            callback.onResult(null);
            return;
        }

        // Note: LargeIconBridge will serve <null> right away if a fetch was previously made and was
        // unsuccessful.
        mIconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx / 2, mDesiredFaviconWidthPx,
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    callback.onResult(icon);
                });
    }

    /**
     * Asynchronously generate favicon for a given url and deliver the result via supplied callback.
     * All fetches are done asynchronously, but the caller should expect a synchronous call in some
     * cases, eg if an icon cannot be retrieved because a corresponding provider is not available.
     *
     * @param url The url to retrieve a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void generateFavicon(@NonNull GURL url, @NonNull Callback<Bitmap> callback) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
            Bitmap icon = mIconGenerator.generateIconForUrl(url);
            callback.onResult(icon);
        });
    }

    /**
     * Clear all cached entries.
     */
    public void resetCache() {
        if (mIconBridge != null) {
            mIconBridge.createCache(MAX_IMAGE_CACHE_SIZE);
        }
    }

    /**
     * Overrides RoundedIconGenerator for testing.
     * @param generator RoundedIconGenerator to use.
     */
    void setRoundedIconGeneratorForTesting(@NonNull RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }
}
