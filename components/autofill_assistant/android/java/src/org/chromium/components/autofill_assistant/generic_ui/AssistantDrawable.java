// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.generic_ui;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.drawable.AssistantDrawableIcon;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/** Represents a view background. */
@JNINamespace("autofill_assistant")
public abstract class AssistantDrawable {
    private static final int INVALID_ICON_ID = -1;

    /** Fetches the drawable. */
    public abstract void getDrawable(Context context, Callback<Drawable> callback);

    @CalledByNative
    public static AssistantDrawable createRectangleShape(
            @Nullable @ColorInt Integer backgroundColor, @Nullable @ColorInt Integer strokeColor,
            int strokeWidthInPixels, int cornerRadiusInPixels) {
        return new AssistantRectangleDrawable(
                backgroundColor, strokeColor, strokeWidthInPixels, cornerRadiusInPixels);
    }

    @CalledByNative
    public static AssistantDrawable createFromUrl(
            ImageFetcher imageFetcher, String url, int widthInPixels, int heightInPixels) {
        return new AssistantBitmapDrawable(imageFetcher, url, widthInPixels, heightInPixels);
    }

    @CalledByNative
    public static AssistantDrawable createFromUrlWithIntrinsicDimensions(
            ImageFetcher imageFetcher, String url) {
        return new AssistantBitmapDrawable(imageFetcher, url);
    }

    /** Returns whether {@code resourceId} is a valid resource identifier. */
    @CalledByNative
    @SuppressWarnings("DiscouragedApi")
    public static boolean isValidDrawableResource(Context context, String resourceId) {
        int drawableId = context.getResources().getIdentifier(
                resourceId, "drawable", context.getPackageName());
        if (drawableId == 0) {
            return false;
        }
        return AppCompatResources.getDrawable(context, drawableId) != null;
    }

    @CalledByNative
    public static AssistantDrawable createFromResource(String resourceId) {
        return new AssistantResourceDrawable(resourceId);
    }

    @CalledByNative
    public static AssistantDrawable createFromIcon(@AssistantDrawableIcon int icon) {
        return new AssistantIconDrawable(icon);
    }

    @CalledByNative
    public static AssistantDrawable createFromBase64(byte[] base64) {
        return new AssistantBase64Drawable(base64);
    }

    @CalledByNative
    public static AssistantDrawable createFromFavicon(
            LargeIconBridge iconBridge, GURL url, int diameterSizeInPixel, boolean forceMonogram) {
        return new AssistantFaviconDrawable(iconBridge, url, diameterSizeInPixel, forceMonogram);
    }

    private static class AssistantRectangleDrawable extends AssistantDrawable {
        private final @Nullable @ColorInt Integer mBackgroundColor;
        private final @Nullable @ColorInt Integer mStrokeColor;
        private final int mStrokeWidthInPixels;
        private final int mCornerRadiusInPixels;

        AssistantRectangleDrawable(@Nullable @ColorInt Integer backgroundColor,
                @Nullable @ColorInt Integer strokeColor, int strokeWidthInPixels,
                int cornerRadiusInPixels) {
            mBackgroundColor = backgroundColor;
            mStrokeColor = strokeColor;
            mStrokeWidthInPixels = strokeWidthInPixels;
            mCornerRadiusInPixels = cornerRadiusInPixels;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            GradientDrawable shape = new GradientDrawable();
            shape.setShape(GradientDrawable.RECTANGLE);
            shape.setCornerRadius(mCornerRadiusInPixels);
            if (mBackgroundColor != null) {
                shape.setColor(mBackgroundColor);
            }
            if (mStrokeColor != null) {
                shape.setStroke(mStrokeWidthInPixels, mStrokeColor);
            }
            callback.onResult(shape);
        }
    }

    private static class AssistantBitmapDrawable extends AssistantDrawable {
        private final ImageFetcher mImageFetcher;
        private final String mUrl;
        private final int mWidthInPixels;
        private final int mHeightInPixels;
        private final boolean mUseInstrinicDimensions;

        AssistantBitmapDrawable(ImageFetcher imageFetcher, String url, int width, int height) {
            mImageFetcher = imageFetcher;
            mUrl = url;
            mWidthInPixels = width;
            mHeightInPixels = height;
            mUseInstrinicDimensions = false;
        }

        AssistantBitmapDrawable(ImageFetcher imageFetcher, String url) {
            mImageFetcher = imageFetcher;
            mUrl = url;
            mWidthInPixels = 0;
            mHeightInPixels = 0;
            mUseInstrinicDimensions = true;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            // TODO(b/143517837) Merge autofill assistant image fetcher UMA names.
            ImageFetcher.Params params = ImageFetcher.Params.create(
                    mUrl, ImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME);
            mImageFetcher.fetchImage(params, result -> {
                if (result != null) {
                    if (!mUseInstrinicDimensions) {
                        result = Bitmap.createScaledBitmap(
                                result, mWidthInPixels, mHeightInPixels, true);
                    }
                    callback.onResult(new BitmapDrawable(context.getResources(), result));
                } else {
                    callback.onResult(null);
                }
            });
        }
    }

    private static class AssistantResourceDrawable extends AssistantDrawable {
        private final String mResourceId;

        AssistantResourceDrawable(String resourceId) {
            mResourceId = resourceId;
        }

        @Override
        @SuppressWarnings("DiscouragedApi")
        public void getDrawable(Context context, Callback<Drawable> callback) {
            int drawableId = context.getResources().getIdentifier(
                    mResourceId, "drawable", context.getPackageName());
            if (drawableId == 0) {
                callback.onResult(null);
            }
            callback.onResult(AppCompatResources.getDrawable(context, drawableId));
        }
    }

    private static class AssistantIconDrawable extends AssistantDrawable {
        private final @AssistantDrawableIcon int mIcon;

        AssistantIconDrawable(@AssistantDrawableIcon int icon) {
            mIcon = icon;
        }

        private int getResourceId() {
            switch (mIcon) {
                case AssistantDrawableIcon.PROGRESSBAR_DEFAULT_INITIAL_STEP:
                    return R.drawable.ic_autofill_assistant_default_progress_start_black_24dp;
                case AssistantDrawableIcon.PROGRESSBAR_DEFAULT_DATA_COLLECTION:
                    return R.drawable.ic_shopping_basket_black_24dp;
                case AssistantDrawableIcon.PROGRESSBAR_DEFAULT_PAYMENT:
                    return R.drawable.ic_payment_black_24dp;
                case AssistantDrawableIcon.PROGRESSBAR_DEFAULT_FINAL_STEP:
                    return R.drawable.ic_check_circle_black_24dp;
                case AssistantDrawableIcon.SITTING_PERSON:
                    return R.drawable.ic_airline_seat_recline_normal_black_24dp;
                case AssistantDrawableIcon.TICKET_STUB:
                    return R.drawable.ic_confirmation_number_black_24dp;
                case AssistantDrawableIcon.SHOPPING_BASKET:
                    return R.drawable.ic_shopping_basket_black_24dp;
                case AssistantDrawableIcon.FAST_FOOD:
                    return R.drawable.ic_fastfood_black_24dp;
                case AssistantDrawableIcon.LOCAL_DINING:
                    return R.drawable.ic_local_dining_black_24dp;
                case AssistantDrawableIcon.COGWHEEL:
                    return R.drawable.ic_settings_black_24dp;
                case AssistantDrawableIcon.KEY:
                    return R.drawable.ic_vpn_key_black_24dp;
                case AssistantDrawableIcon.CAR:
                    return R.drawable.ic_directions_car_black_24dp;
                case AssistantDrawableIcon.GROCERY:
                    return R.drawable.ic_grocery_black_24dp;
                case AssistantDrawableIcon.VISIBILITY_ON:
                    return R.drawable.ic_visibility_black;
                case AssistantDrawableIcon.VISIBILITY_OFF:
                    return R.drawable.ic_visibility_off_black;
            }

            return INVALID_ICON_ID;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            int resourceId = getResourceId();
            callback.onResult(resourceId == INVALID_ICON_ID
                            ? null
                            : AppCompatResources.getDrawable(context, resourceId));
        }
    }

    private static class AssistantBase64Drawable extends AssistantDrawable {
        private final byte[] mBase64;

        AssistantBase64Drawable(byte[] base64) {
            mBase64 = base64;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            Bitmap icon = BitmapFactory.decodeByteArray(mBase64, /* offset= */ 0, mBase64.length);
            callback.onResult(new BitmapDrawable(context.getResources(), icon));
        }
    }

    private static class AssistantFaviconDrawable extends AssistantDrawable {
        private final LargeIconBridge mIconBridge;
        private final GURL mUrl;
        private final int mDiameterSizeInPixel;
        private final Boolean mForceMonogram;

        AssistantFaviconDrawable(LargeIconBridge iconBridge, GURL url, int diameterSizeInPixel,
                boolean forceMonogram) {
            mIconBridge = iconBridge;
            mUrl = url;
            mDiameterSizeInPixel = diameterSizeInPixel;
            mForceMonogram = forceMonogram;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            mIconBridge.getLargeIconForUrl(mUrl, mDiameterSizeInPixel,
                    (@Nullable Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                            int iconType) -> {
                        boolean useMonogram = mForceMonogram || icon == null;

                        Drawable drawable = useMonogram
                                ? getMonogramDrawable(mUrl, fallbackColor, context)
                                : getDrawableFromFavicon(
                                        icon, context.getResources(), mDiameterSizeInPixel);
                        callback.onResult(drawable);
                    });
        }

        private Drawable getMonogramDrawable(GURL url, int fallbackColor, Context context) {
            float fontSize = mDiameterSizeInPixel * 7f / 10f;
            RoundedIconGenerator roundedIconGenerator = new RoundedIconGenerator(
                    mDiameterSizeInPixel, mDiameterSizeInPixel, mDiameterSizeInPixel / 2,
                    context.getColor(R.color.default_favicon_background_color), fontSize);
            roundedIconGenerator.setBackgroundColor(fallbackColor);
            Bitmap icon = roundedIconGenerator.generateIconForUrl(url);
            return new BitmapDrawable(context.getResources(), icon);
        }

        private Drawable getDrawableFromFavicon(Bitmap icon, Resources resources, int iconSize) {
            return ViewUtils.createRoundedBitmapDrawable(resources,
                    Bitmap.createScaledBitmap(icon, iconSize, iconSize, false),
                    resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius));
        }
    }
}
