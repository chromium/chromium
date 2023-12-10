// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/**
 * This class contains functions related to adding shortcuts to the Android Home screen. These
 * shortcuts are used to either open a page in the main browser or open a web app.
 */
public class WebappsIconUtils {
    private static final String TAG = "WebappsIconUtils";

    // These sizes are from the Material spec for icons:
    // https://www.google.com/design/spec/style/icons.html#icons-product-icons
    private static final float MAX_INNER_SIZE_RATIO = 1.25f;
    private static final float ICON_PADDING_RATIO = 2.0f / 44.0f;
    private static final float ICON_CORNER_RADIUS_RATIO = 1.0f / 16.0f;
    private static final float GENERATED_ICON_PADDING_RATIO = 1.0f / 12.0f;
    private static final float GENERATED_ICON_FONT_SIZE_RATIO = 1.0f / 3.0f;

    // Constants for figuring out the amount of padding required to transform a web manifest
    // maskable icon to an Android adaptive icon.
    //
    // The web standard for maskable icons specifies a larger safe zone inside the icon
    // than Android adaptive icons define. Therefore we need to pad the image so that
    // the maskable icon's safe zone is reduced to the dimensions expected by Android. See
    // https://github.com/w3c/manifest/issues/555#issuecomment-404097653.
    //
    // The *_RATIO variables give the diameter of the safe zone divided by the width of the icon.
    // Sources:
    // - https://www.w3.org/TR/appmanifest/#icon-masks
    // - https://medium.com/google-design/designing-adaptive-icons-515af294c783
    //
    // We subtract 1 from the scaling factor to give the amount we need to increase by, then divide
    // it by two to get the amount of padding that we will add to both sides.
    private static final float MASKABLE_SAFE_ZONE_RATIO = 4.0f / 5.0f;
    private static final float ADAPTIVE_SAFE_ZONE_RATIO = 66.0f / 108.0f;

    private static final float MASKABLE_TO_ADAPTIVE_SCALING_FACTOR =
            MASKABLE_SAFE_ZONE_RATIO / ADAPTIVE_SAFE_ZONE_RATIO;

    private static final float MASKABLE_ICON_PADDING_RATIO =
            (MASKABLE_TO_ADAPTIVE_SCALING_FACTOR - 1.0f) / 2.0f;

    private static final float SHORTCUT_ICON_IDEAL_SIZE_DP = 48;

    @RequiresApi(Build.VERSION_CODES.O)
    @CalledByNative
    public static Bitmap generateAdaptiveIconBitmap(Bitmap bitmap) {
        Bitmap padded = createHomeScreenIconFromWebIcon(bitmap, true);
        Icon adaptiveIcon = Icon.createWithAdaptiveBitmap(padded);
        AdaptiveIconDrawable adaptiveIconDrawable =
                (AdaptiveIconDrawable)
                        adaptiveIcon.loadDrawable(ContextUtils.getApplicationContext());

        Bitmap result =
                Bitmap.createBitmap(
                        adaptiveIconDrawable.getIntrinsicWidth(),
                        adaptiveIconDrawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        adaptiveIconDrawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        adaptiveIconDrawable.draw(canvas);

        return result;
    }

    /**
     * Adapts a website's icon (e.g. favicon or touch icon) to make it suitable for the home screen.
     * This involves adding padding if the icon is a full sized square.
     *
     * @param webIcon The website's favicon or touch icon.
     * @param maskable Whether the icon is suitable for creating an adaptive icon.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    public static Bitmap createHomeScreenIconFromWebIcon(Bitmap webIcon, boolean maskable) {
        // getLauncherLargeIconSize() is just a guess at the launcher icon size, and is often
        // wrong -- the launcher can show icons at any size it pleases. Instead of resizing the
        // icon to the supposed launcher size and then having the launcher resize the icon again,
        // just leave the icon at its original size and let the launcher do a single rescaling.
        // Unless the icon is much too big; then scale it down here too.
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        int maxInnerSize = Math.round(am.getLauncherLargeIconSize() * MAX_INNER_SIZE_RATIO);
        int innerSize = Math.min(maxInnerSize, Math.max(webIcon.getWidth(), webIcon.getHeight()));

        Rect innerBounds = new Rect(0, 0, innerSize, innerSize);
        int padding = 0;

        if (maskable) {
            // See comments for MASKABLE_ICON_PADDING_RATIO.
            padding = Math.round(MASKABLE_ICON_PADDING_RATIO * innerSize);
        } else if (shouldPadIcon(webIcon)) {
            // Draw the icon with padding around it if all four corners are not transparent.
            padding = Math.round(ICON_PADDING_RATIO * innerSize);
        }

        int outerSize = 2 * padding + innerSize;
        innerBounds.offset(padding, padding);

        Bitmap bitmap;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.e(TAG, "OutOfMemoryError while creating bitmap for home screen icon.");
            return webIcon;
        }

        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setFilterBitmap(true);
        canvas.drawBitmap(webIcon, null, innerBounds, paint);

        return bitmap;
    }

    /**
     * Returns the ideal size for an icon representing a web app. This size is used on app banners,
     * the Android Home screen, and in Android's recent tasks list, among other places.
     *
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the icon should have.
     */
    public static int getIdealHomescreenIconSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_home_screen_icon_size);
    }

    /**
     * Returns the minimum size for an icon representing a web app. This size is used on app
     * banners, the Android Home screen, and in Android's recent tasks list, among other places.
     *
     * @param context Context to pull resources from.
     * @return the lower bound of the size which the icon should have in pixels.
     */
    public static int getMinimumHomescreenIconSizeInPx(Context context) {
        float sizeInPx = context.getResources().getDimension(R.dimen.webapp_home_screen_icon_size);
        float density = context.getResources().getDisplayMetrics().density;
        float idealIconSizeInDp = sizeInPx / density;

        return Math.round(idealIconSizeInDp * (density - 1));
    }

    /**
     * Returns the ideal size for an image displayed on a web app's splash screen.
     *
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the image should have.
     */
    public static int getIdealSplashImageSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_splash_image_size_ideal);
    }

    /**
     * Returns the minimum size for an image displayed on a web app's splash screen.
     *
     * @param context Context to pull resources from.
     * @return the lower bound of the size which the image should have in pixels.
     */
    public static int getMinimumSplashImageSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_splash_image_size_minimum);
    }

    /**
     * Returns the ideal size for a monochrome icon of a WebAPK.
     *
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the monochrome icon should have.
     */
    public static int getIdealMonochromeIconSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapk_monochrome_icon_size);
    }

    /**
     * Returns the ideal size for an adaptive launcher icon of a WebAPK.
     *
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the adaptive launcher icon should have.
     */
    public static int getIdealAdaptiveLauncherIconSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapk_adaptive_icon_size);
    }

    /**
     * Returns the ideal size for prompt UI icon corner radius.
     *
     * @return the dimensions in pixels which the prompt UI should use as the corner radius.
     */
    @CalledByNative
    public static int getIdealIconCornerRadiusPxForPromptUI() {
        Context context = ContextUtils.getApplicationContext();
        return context.getResources().getDimensionPixelSize(R.dimen.webapk_prompt_ui_icon_radius);
    }

    /** Check the running Android version supports adaptive icon (i.e. API level >= 26) */
    public static boolean doesAndroidSupportMaskableIcons() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
    }

    /**
     * Returns whether the given icon matches the size requirements to be used on the home screen.
     *
     * @param width Icon width, in pixels.
     * @param height Icon height, in pixels.
     * @return whether the given icon matches the size requirements to be used on the home screen.
     */
    @CalledByNative
    public static boolean isIconLargeEnoughForLauncher(int width, int height) {
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int minimalSize = am.getLauncherLargeIconSize() / 2;
        return width >= minimalSize && height >= minimalSize;
    }

    /**
     * Generates a generic icon to be used in the launcher. This is just a rounded rectangle with a
     * letter in the middle taken from the website's domain name.
     *
     * @param url URL of the shortcut.
     * @param red Red component of the dominant icon color.
     * @param green Green component of the dominant icon color.
     * @param blue Blue component of the dominant icon color.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    @CalledByNative
    public static Bitmap generateHomeScreenIcon(GURL url, int red, int green, int blue) {
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int outerSize = am.getLauncherLargeIconSize();
        final int iconDensity = am.getLauncherLargeIconDensity();

        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
            return null;
        }

        Canvas canvas = new Canvas(bitmap);

        // Draw the drop shadow.
        int padding = (int) (GENERATED_ICON_PADDING_RATIO * outerSize);
        Rect outerBounds = new Rect(0, 0, outerSize, outerSize);
        Bitmap iconShadow =
                getBitmapFromResourceId(context, R.mipmap.shortcut_icon_shadow, iconDensity);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(iconShadow, null, outerBounds, paint);

        // Draw the rounded rectangle and letter.
        int innerSize = outerSize - 2 * padding;
        int cornerRadius = Math.round(ICON_CORNER_RADIUS_RATIO * outerSize);
        int fontSize = Math.round(GENERATED_ICON_FONT_SIZE_RATIO * outerSize);
        int color = Color.rgb(red, green, blue);
        RoundedIconGenerator generator =
                new RoundedIconGenerator(innerSize, innerSize, cornerRadius, color, fontSize);
        Bitmap icon = generator.generateIconForUrl(url);
        if (icon == null) return null; // Bookmark URL does not have a domain.
        canvas.drawBitmap(icon, padding, padding, null);

        return bitmap;
    }

    /**
     * Returns an array of sizes which describe the ideal size and minimum size of the Home screen
     * icon and the ideal and minimum sizes of the splash screen image in that order.
     */
    @CalledByNative
    private static int[] getIconSizes() {
        Context context = ContextUtils.getApplicationContext();
        // This ordering must be kept up to date with the C++ WebappsIconUtils.
        return new int[] {
            getIdealHomescreenIconSizeInPx(context),
            getMinimumHomescreenIconSizeInPx(context),
            getIdealSplashImageSizeInPx(context),
            getMinimumSplashImageSizeInPx(context),
            getIdealMonochromeIconSizeInPx(context),
            getIdealAdaptiveLauncherIconSizeInPx(context),
            ViewUtils.dpToPx(context, SHORTCUT_ICON_IDEAL_SIZE_DP)
        };
    }

    /**
     * Returns true if we should add padding to this icon. We use a heuristic that if the pixels in
     * all four corners of the icon are not transparent, we assume the icon is square and maximally
     * sized, i.e. in need of padding. Otherwise, no padding is added.
     */
    private static boolean shouldPadIcon(Bitmap icon) {
        int maxX = icon.getWidth() - 1;
        int maxY = icon.getHeight() - 1;

        if ((Color.alpha(icon.getPixel(0, 0)) != 0)
                && (Color.alpha(icon.getPixel(maxX, maxY)) != 0)
                && (Color.alpha(icon.getPixel(0, maxY)) != 0)
                && (Color.alpha(icon.getPixel(maxX, 0)) != 0)) {
            return true;
        }
        return false;
    }

    private static int getSizeFromResourceInPx(Context context, int resource) {
        return Math.round(context.getResources().getDimension(resource));
    }

    private static Bitmap getBitmapFromResourceId(Context context, int id, int density) {
        Drawable drawable =
                ApiCompatibilityUtils.getDrawableForDensity(context.getResources(), id, density);

        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bd = (BitmapDrawable) drawable;
            return bd.getBitmap();
        }
        assert false : "The drawable was not a bitmap drawable as expected";
        return null;
    }
}
