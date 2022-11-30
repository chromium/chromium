// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/**
 * An abstract class that provides the current theme color.
 */
public abstract class ThemeColorProvider {
    /**
     * An interface to be notified about changes to the theme color.
     */
    public interface ThemeColorObserver {
        /**
         * @param color The new color the observer should use.
         * @param shouldAnimate Whether the change of color should be animated.
         */
        void onThemeColorChanged(int color, boolean shouldAnimate);
    }

    /**
     * An interface to be notified about changes to the tint.
     */
    public interface TintObserver {
        /**
         * @param tint The new tint the observer should use.
         * @param brandedColorScheme The {@link BrandedColorScheme} the observer should use.
         */
        void onTintChanged(ColorStateList tint, @BrandedColorScheme int brandedColorScheme);
    }

    /** Current primary color. */
    private int mPrimaryColor;

    /** The current {@link BrandedColorScheme}. */
    private @Nullable @BrandedColorScheme Integer mBrandedColorScheme;

    /** The current tint. */
    private ColorStateList mTint;

    /** List of {@link ThemeColorObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<ThemeColorObserver> mThemeColorObservers;

    /** List of {@link TintObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<TintObserver> mTintObservers;

    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public ThemeColorProvider(Context context) {
        mThemeColorObservers = new ObserverList<ThemeColorObserver>();
        mTintObservers = new ObserverList<TintObserver>();
        mTint = ThemeUtils.getThemedToolbarIconTint(context, BrandedColorScheme.APP_DEFAULT);
    }

    /**
     * @param observer Adds a {@link ThemeColorObserver} that will be notified when the theme color
     *                 changes. This method does not trigger the observer.
     */
    public void addThemeColorObserver(ThemeColorObserver observer) {
        mThemeColorObservers.addObserver(observer);
    }

    /**
     * @param observer Removes the observer so it no longer receives theme color changes.
     */
    public void removeThemeColorObserver(ThemeColorObserver observer) {
        mThemeColorObservers.removeObserver(observer);
    }

    /**
     * @param observer Adds a {@link TintObserver} that will be notified when the tint changes. This
     *                 method does not trigger the observer.
     */
    public void addTintObserver(TintObserver observer) {
        mTintObservers.addObserver(observer);
    }

    /**
     * @param observer Removes the observer so it no longer receives tint changes.
     */
    public void removeTintObserver(TintObserver observer) {
        mTintObservers.removeObserver(observer);
    }

    /**
     * @return The current theme color of this provider.
     */
    @ColorInt
    public int getThemeColor() {
        return mPrimaryColor;
    }

    /**
     * @return The current tint of this provider.
     */
    public ColorStateList getTint() {
        return mTint;
    }

    /**
     * @return The current {@link BrandedColorScheme} of this provider.
     */
    public @BrandedColorScheme int getBrandedColorScheme() {
        return mBrandedColorScheme != null ? mBrandedColorScheme : BrandedColorScheme.APP_DEFAULT;
    }

    /**
     * Clears out the observer lists.
     */
    public void destroy() {
        mThemeColorObservers.clear();
        mTintObservers.clear();
    }

    protected void updatePrimaryColor(int color, boolean shouldAnimate) {
        if (mPrimaryColor == color) return;
        mPrimaryColor = color;
        for (ThemeColorObserver observer : mThemeColorObservers) {
            observer.onThemeColorChanged(color, shouldAnimate);
        }
    }

    protected void updateTint(
            @NonNull ColorStateList tint, @BrandedColorScheme int brandedColorScheme) {
        if (tint == mTint) return;
        mTint = tint;
        mBrandedColorScheme = brandedColorScheme;

        for (TintObserver observer : mTintObservers) {
            observer.onTintChanged(tint, brandedColorScheme);
        }
    }
}
