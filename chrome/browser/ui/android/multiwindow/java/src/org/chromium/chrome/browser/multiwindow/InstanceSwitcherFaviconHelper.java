// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Stateful helper for fetching and setting favicons in multi-instance UI. */
@NullMarked
public class InstanceSwitcherFaviconHelper {
    private final Context mContext;
    private final int mMinIconSizeDp;
    private final int mDisplayedIconSize;
    private final Drawable mIncognitoFavicon;
    private final Drawable mGlobeFavicon;
    private final LargeIconBridge mLargeIconBridge;
    private final RoundedIconGenerator mIconGenerator;

    /* package */ InstanceSwitcherFaviconHelper(Context context, LargeIconBridge iconBridge) {
        mContext = context;
        mLargeIconBridge = iconBridge;
        Resources res = context.getResources();
        mMinIconSizeDp = (int) res.getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = res.getDimensionPixelSize(R.dimen.default_favicon_size);
        mIncognitoFavicon =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? mContext.getResources()
                                .getDrawable(
                                        R.drawable.ic_incognito_circle_fill_24dp,
                                        mContext.getTheme())
                        : getTintedIcon(R.drawable.ic_incognito_fill_24dp);
        mGlobeFavicon = getTintedIcon(R.drawable.ic_globe_24dp);
        mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(context);
    }

    /* package */ void setFavicon(
            PropertyModel model,
            PropertyModel.WritableObjectPropertyKey<Drawable> faviconKey,
            InstanceInfo item) {
        int incognitoTabCount = UiUtils.recoverableIncognitoTabCount(item);
        int totalTabCount = UiUtils.totalTabCount(item);
        if (totalTabCount == 0 || UiUtils.isInitialNonIncognitoWindow(item, totalTabCount)) {
            model.set(faviconKey, mGlobeFavicon);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            model.set(faviconKey, mIncognitoFavicon);
        } else {
            GURL url = new GURL(item.url);
            mLargeIconBridge.getLargeIconForUrl(
                    url,
                    mMinIconSizeDp,
                    (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                        model.set(faviconKey, createIconDrawable(item.url, icon, fallbackColor));
                    });
        }
    }

    private Drawable getTintedIcon(@DrawableRes int drawableId) {
        return org.chromium.ui.UiUtils.getTintedDrawable(
                mContext, drawableId, R.color.default_icon_color_tint_list);
    }

    private Drawable createIconDrawable(String url, @Nullable Bitmap icon, int fallbackColor) {
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(url);
        } else {
            icon = Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, true);
        }
        return new BitmapDrawable(mContext.getResources(), icon);
    }
}
