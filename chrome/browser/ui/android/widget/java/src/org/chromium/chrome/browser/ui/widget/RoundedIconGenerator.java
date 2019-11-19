// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.RectF;
import android.text.TextPaint;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.UrlUtilities;

import java.net.URI;
import java.util.Locale;

/**
 * Generator for transparent icons containing a rounded rectangle with a given background color,
 * having a centered character drawn on top of it.
 */
public class RoundedIconGenerator {
    private static final String TAG = RoundedIconGenerator.class.getSimpleName();

    private final int mIconWidthPx;
    private final int mIconHeightPx;
    private final int mCornerRadiusPx;

    private final RectF mBackgroundRect;

    private final Paint mBackgroundPaint;
    private final TextPaint mTextPaint;

    private final float mTextHeight;
    private final float mTextYOffset;

    /**
     * Constructs the generator and initializes the common members based on the display density.
     *
     * @param res The resources used to convert sizes to px.
     * @param iconWidthDp The width of the generated icon in dp.
     * @param iconHeightDp The height of the generated icon in dp.
     * @param cornerRadiusDp The radius of the corners in the icon in dp.
     * @param backgroundColor Color with which the rounded rectangle should be drawn.
     * @param textSizeDp Size at which the text should be drawn in dp.
     */
    public RoundedIconGenerator(Resources res, int iconWidthDp, int iconHeightDp,
            int cornerRadiusDp, int backgroundColor, int textSizeDp) {
        this((int) (res.getDisplayMetrics().density * iconWidthDp),
                (int) (res.getDisplayMetrics().density * iconHeightDp),
                (int) (res.getDisplayMetrics().density * cornerRadiusDp), backgroundColor,
                res.getDisplayMetrics().density * textSizeDp);
    }

    /**
     * Constructs the generator and initializes the common members ignoring display density.
     *
     * @param iconWidthPx The width of the generated icon in pixels.
     * @param iconHeightPx The height of the generated icon in pixels.
     * @param cornerRadiusPx The radius of the corners in the icon in pixels.
     * @param backgroundColor Color at which the rounded rectangle should be drawn.
     * @param textSizePx Size at which the text should be drawn in pixels.
     */
    public RoundedIconGenerator(int iconWidthPx, int iconHeightPx, int cornerRadiusPx,
            int backgroundColor, float textSizePx) {
        mIconWidthPx = iconWidthPx;
        mIconHeightPx = iconHeightPx;
        mCornerRadiusPx = cornerRadiusPx;

        mBackgroundRect = new RectF(0, 0, mIconWidthPx, mIconHeightPx);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(backgroundColor);

        mTextPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setColor(Color.WHITE);
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setTextSize(textSizePx);

        FontMetrics textFontMetrics = mTextPaint.getFontMetrics();
        mTextHeight = (float) Math.ceil(textFontMetrics.bottom - textFontMetrics.top);
        mTextYOffset = -textFontMetrics.top;
    }

    /**
     * Sets the background color to use when generating icons.
     */
    public void setBackgroundColor(int color) {
        mBackgroundPaint.setColor(color);
    }

    /**
     * Generates an icon based on |text| (using the first character).
     *
     * @param text The text to render the first character of on the icon.
     * @return The generated icon.
     */
    public Bitmap generateIconForText(String text) {
        Bitmap icon = Bitmap.createBitmap(mIconWidthPx, mIconHeightPx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(icon);

        canvas.drawRoundRect(mBackgroundRect, mCornerRadiusPx, mCornerRadiusPx, mBackgroundPaint);

        int length = Math.min(1, text.length());
        String displayText = text.substring(0, length).toUpperCase(Locale.getDefault());
        float textWidth = mTextPaint.measureText(displayText);

        canvas.drawText(displayText, (mIconWidthPx - textWidth) / 2f,
                Math.round(
                        (Math.max(mIconHeightPx, mTextHeight) - mTextHeight) / 2.0f + mTextYOffset),
                mTextPaint);

        return icon;
    }

    /**
     * Returns a Bitmap representing the icon to be used for |url|.
     *
     * @param url URL for which the icon should be generated.
     * @param includePrivateRegistries Should private registries be considered as TLDs?
     * @return The generated icon, or NULL if |url| is empty or the domain cannot be resolved.
     */
    @Nullable
    public Bitmap generateIconForUrl(String url, boolean includePrivateRegistries) {
        if (TextUtils.isEmpty(url)) return null;

        String text = getIconTextForUrl(url, includePrivateRegistries);
        if (TextUtils.isEmpty(text)) return null;

        return generateIconForText(text);
    }

    /**
     * Returns a Bitmap representing the icon to be used for |url|. Private registries such
     * as "appspot.com" will not be considered as effective TLDs.
     *
     * @TODO(beverloo) Update all call-sites of rounded icons to be explicit about whether
     * private registries should be considered, matching the getDomainAndRegistry requirements.
     * See https://crbug.com/458104.
     *
     * @param url URL for which the icon should be generated.
     * @return The generated icon, or NULL if |url| is empty or the domain cannot be resolved.
     */
    @Nullable
    public Bitmap generateIconForUrl(String url) {
        return generateIconForUrl(url, false);
    }

    /**
     * Returns the text which should be used for generating a rounded icon based on |url|.
     *
     * @param url URL to consider when getting the icon's text.
     * @param includePrivateRegistries Should private registries be considered as TLDs?
     * @return The text to use on the rounded icon, or NULL if |url| is empty or the domain cannot
     *         be resolved.
     */
    @Nullable
    @VisibleForTesting
    public static String getIconTextForUrl(String url, boolean includePrivateRegistries) {
        String domain = UrlUtilities.getDomainAndRegistry(url, includePrivateRegistries);
        if (!TextUtils.isEmpty(domain)) return domain;

        // Special-case chrome:// and chrome-native:// URLs.
        if (url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX)) {
            return UrlConstants.CHROME_SCHEME;
        }

        // Use the host component of |url| when it can be parsed as a URI.
        try {
            URI uri = new URI(url);
            if (!TextUtils.isEmpty(uri.getHost())) {
                return uri.getHost();
            }
        } catch (Exception e) {
            Log.w(TAG, "Unable to parse the URL for generating an icon: " + url);
        }

        return url;
    }
}
