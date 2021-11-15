// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.content.res.TypedArray;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import com.google.android.material.elevation.ElevationOverlayProvider;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import org.chromium.ui.util.AttrUtils;

import java.io.IOException;

/**
 * A subclass of GradientDrawable that allows us to both control shape with an attribute, as well
 * as the surface color based on elevation. If no elevation is specified a default of 0 will be
 * used. This will result in the same color as Surface-0. If this is desired, .it is likely easier
 * to use ?attr/colorSurface.
 *
 * Example
 * <org.chromium.components.browser_ui.widget.SurfaceColorDrawable
 *     xmlns:android="http://schemas.android.com/apk/res/android"
 *     xmlns:app="http://schemas.android.com/apk/res-auto"
 *     android:shape="oval"
 *     app:surfaceElevation="@dimen/default_elevation_1"/>
 */
public class SurfaceColorDrawable extends GradientDrawable {
    private @Px float mElevation;
    private float mDensity;

    @Override
    public void inflate(@NonNull Resources resources, @NonNull XmlPullParser parser,
            @NonNull AttributeSet attrs, @Nullable Theme theme)
            throws XmlPullParserException, IOException {
        // All attributes must be read before we call super#inflate, which will advance the parser
        // which seems to change what attrs is pointing at.
        final TypedArray typedArray =
                resources.obtainAttributes(attrs, R.styleable.SurfaceColorDrawable);
        mElevation = typedArray.getDimensionPixelSize(
                R.styleable.SurfaceColorDrawable_surfaceElevation, 0);
        typedArray.recycle();

        super.inflate(resources, parser, attrs, theme);

        mDensity = resources.getDisplayMetrics().density;

        // Pretty much always no-ops, as theme is usually null here.
        if (theme != null) {
            onNonNullTheme(theme);
        }
    }

    @Override
    public boolean canApplyTheme() {
        return true;
    }

    @Override
    public void applyTheme(@NonNull Theme theme) {
        super.applyTheme(theme);
        onNonNullTheme(theme);
    }

    private void onNonNullTheme(@NonNull Theme theme) {
        boolean elevationOverlayEnabled =
                AttrUtils.resolveBoolean(theme, R.attr.elevationOverlayEnabled);
        final @ColorInt int elevationOverlayColor =
                AttrUtils.resolveColor(theme, R.attr.elevationOverlayColor);
        final @ColorInt int elevationOverlayAccentColor =
                AttrUtils.resolveColor(theme, R.attr.elevationOverlayAccentColor);
        final @ColorInt int colorSurface = AttrUtils.resolveColor(theme, R.attr.colorSurface);

        ElevationOverlayProvider elevationOverlayProvider =
                new ElevationOverlayProvider(elevationOverlayEnabled, elevationOverlayColor,
                        elevationOverlayAccentColor, colorSurface, mDensity);
        final @ColorInt int color =
                elevationOverlayProvider.compositeOverlayWithThemeSurfaceColorIfNeeded(mElevation);
        setTint(color);
    }
}
