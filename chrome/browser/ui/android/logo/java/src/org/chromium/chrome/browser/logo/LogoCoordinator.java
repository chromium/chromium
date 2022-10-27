// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.ref.WeakReference;

/** Coordinator used to fetch and load logo image for Start surface and NTP.*/
public class LogoCoordinator implements TemplateUrlServiceObserver {
    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final PropertyModel mLogoModel;
    private final LogoView mLogoView;
    private final Callback<Logo> mOnLogoAvailableRunnable;
    private final Runnable mOnCachedLogoRevalidatedRunnable;
    private final Context mContext;

    private LogoDelegateImpl mLogoDelegate;
    private Profile mProfile;
    private boolean mHasLogoLoadedForCurrentSearchEngine;
    private boolean mShouldFetchDoodle;
    private boolean mIsParentSurfaceShown; // This value should always be true when this class
                                           // is used by NTP.
    private boolean mShouldShowLogo;
    private boolean mIsNativeInitialized;
    private boolean mIsLoadPending;

    // The default logo is shared across all NTPs.
    private static WeakReference<Bitmap> sDefaultLogo;
    private static @ColorInt int sDefaultLogoTint;

    private final ObserverList<VisibilityObserver> mVisibilityObservers = new ObserverList<>();

    /** Interface for the observers of the logo visibility change. */
    public interface VisibilityObserver {
        void onLogoVisibilityChanged();
    }

    /**
     * Creates a LogoCoordinator object.
     *
     * @param context Used to load colors and resources.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoView The view that shows the search provider logo.
     * @param shouldFetchDoodle Whether to fetch doodle if there is.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param onCachedLogoRevalidatedRunnable The runnable for when cached logo is revalidated.
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             is true when this class is used by NTP; while used by Start,
     *                             it's only true on Start homepage.
     */
    public LogoCoordinator(Context context, Callback<LoadUrlParams> logoClickedCallback,
            LogoView logoView, boolean shouldFetchDoodle, Callback<Logo> onLogoAvailableCallback,
            Runnable onCachedLogoRevalidatedRunnable, boolean isParentSurfaceShown) {
        mLogoClickedCallback = logoClickedCallback;
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mLogoView = logoView;
        mShouldFetchDoodle = shouldFetchDoodle;
        mOnLogoAvailableRunnable = onLogoAvailableCallback;
        mOnCachedLogoRevalidatedRunnable = onCachedLogoRevalidatedRunnable;
        mIsParentSurfaceShown = isParentSurfaceShown;
        mContext = context;
        PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());
    }

    /**
     * Initialize the coordinator with the components that had native initialization dependencies,
     * i.e. Profile..
     */
    public void initWithNative() {
        if (mIsNativeInitialized) return;

        mIsNativeInitialized = true;
        mProfile = Profile.getLastUsedRegularProfile();
        updateVisibility();

        if (mShouldShowLogo) {
            showSearchProviderInitialView();
            if (mIsLoadPending) loadSearchProviderLogo(/*animationEnabled=*/false);
        }

        TemplateUrlServiceFactory.get().addObserver(this);
    }

    /** Update the logo based on default search engine changes.*/
    @Override
    public void onTemplateURLServiceChanged() {
        loadSearchProviderLogoWithAnimation();
    }

    /** Force to load the search provider logo with animation enabled.*/
    public void loadSearchProviderLogoWithAnimation() {
        mHasLogoLoadedForCurrentSearchEngine = false;
        maybeLoadSearchProviderLogo(mIsParentSurfaceShown, /*shouldDestroyDelegate=*/false, true);
    }

    /**
     * If it's on Start surface homepage or on NTP, load search provider logo; If it's not on Start
     * surface homepage, destroy mLogoDelegate.
     *
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             should always be true when this class is used by NTP.
     * @param shouldDestroyDelegate Whether to destroy delegate for saving memory. This value should
     *                              always be false when this class is used by NTP.
     *                              TODO(crbug.com/1315676): Remove this variable once the refactor
     *                              is launched and StartSurfaceState is removed. Now we check this
     *                              because there are some intermediate StartSurfaceStates,
     *                              i.e. SHOWING_START.
     * @param animationEnabled Whether to enable the fade in animation.
     */
    public void maybeLoadSearchProviderLogo(
            boolean isParentSurfaceShown, boolean shouldDestroyDelegate, boolean animationEnabled) {
        assert !isParentSurfaceShown || !shouldDestroyDelegate;

        mIsParentSurfaceShown = isParentSurfaceShown;
        updateVisibility();

        if (mShouldShowLogo) {
            if (mProfile != null) {
                loadSearchProviderLogo(animationEnabled);
            } else {
                mIsLoadPending = true;
            }
        } else if (shouldDestroyDelegate && mLogoDelegate != null) {
            mHasLogoLoadedForCurrentSearchEngine = false;
            // Destroy |mLogoDelegate| when hiding Start surface homepage to save memory.
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
    }

    /** Cleans up any code as necessary.*/
    public void destroy() {
        if (mLogoDelegate != null) {
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }

        if (mLogoView != null) {
            mLogoModel.set(LogoProperties.DESTROY, true);
        }

        if (mIsNativeInitialized) {
            TemplateUrlServiceFactory.get().removeObserver(this);
        }
    }

    /** Returns the logo view.*/
    public LogoView getView() {
        return mLogoView;
    }

    /** Jumps to the end of the logo view's cross-fading animation, if any.*/
    public void endFadeAnimation() {
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setAlpha(float alpha) {
        mLogoModel.set(LogoProperties.ALPHA, alpha);
    }

    /**
     * Sets the top margin of the logo view.
     *
     * @param topMargin The expected top margin.
     */
    public void setTopMargin(int topMargin) {
        mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, topMargin);
    }

    /**
     * Sets the bottom margin of the logo view.
     *
     * @param bottomMargin The expected bottom margin.
     */
    public void setBottomMargin(int bottomMargin) {
        mLogoModel.set(LogoProperties.LOGO_BOTTOM_MARGIN, bottomMargin);
    }

    /** Returns whether LogoView is visible.*/
    public boolean isLogoVisible() {
        return mShouldShowLogo && mLogoModel.get(LogoProperties.VISIBILITY);
    }

    /**
     * Add {@link Observer} object.
     * @param observer Observer object monitoring logo visibility.
     */
    public void addObserver(VisibilityObserver observer) {
        mVisibilityObservers.addObserver(observer);
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring logo visibility.
     */
    public void removeObserver(VisibilityObserver observer) {
        mVisibilityObservers.removeObserver(observer);
    }

    /**
     * Load the search provider logo on Start surface.
     *
     * @param animationEnabled Whether to enable the fade in animation.
     */
    private void loadSearchProviderLogo(boolean animationEnabled) {
        // If logo is already updated for the current search provider, or profile is null or off the
        // record, don't bother loading the logo image.
        if (mHasLogoLoadedForCurrentSearchEngine || mProfile == null || !mShouldShowLogo) return;

        mHasLogoLoadedForCurrentSearchEngine = true;
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, animationEnabled);
        showSearchProviderInitialView();

        // If default search engine is google and doodle is not supported, doesn't bother to fetch
        // logo image.
        if (TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle() && !mShouldFetchDoodle) {
            return;
        }

        if (mLogoDelegate == null) {
            mLogoDelegate = new LogoDelegateImpl(mLogoClickedCallback, mLogoView, mProfile);
        }

        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null) {
                    if (fromCache) {
                        // There is no cached logo. Wait until we know whether there's a fresh
                        // one before making any further decisions.
                        return;
                    }
                    mLogoModel.set(
                            LogoProperties.DEFAULT_GOOGLE_LOGO, getDefaultGoogleLogo(mContext));
                }
                mLogoModel.set(LogoProperties.LOGO_DELEGATE, mLogoDelegate);
                mLogoModel.set(LogoProperties.UPDATED_LOGO, logo);

                if (mOnLogoAvailableRunnable != null) mOnLogoAvailableRunnable.onResult(logo);
            }

            @Override
            public void onCachedLogoRevalidated() {
                if (mOnCachedLogoRevalidatedRunnable != null) {
                    mOnCachedLogoRevalidatedRunnable.run();
                }
            }
        });
    }

    private void showSearchProviderInitialView() {
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO, getDefaultGoogleLogo(mContext));
        mLogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
    }

    private void updateVisibility() {
        boolean doesDSEHaveLogo = mIsNativeInitialized
                ? TemplateUrlServiceFactory.get().doesDefaultSearchEngineHaveLogo()
                : SharedPreferencesManager.getInstance().readBoolean(
                        APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
        mShouldShowLogo = mIsParentSurfaceShown && doesDSEHaveLogo;
        mLogoModel.set(LogoProperties.VISIBILITY, mShouldShowLogo);
        for (VisibilityObserver observer : mVisibilityObservers) {
            observer.onLogoVisibilityChanged();
        }
    }

    /**
     * Get the default Google logo if available.
     * @param context Used to load colors and resources.
     * @return The default Google logo.
     */
    public static Bitmap getDefaultGoogleLogo(Context context) {
        if (!TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) return null;

        Bitmap defaultLogo = sDefaultLogo == null ? null : sDefaultLogo.get();
        final int tint = context.getColor(R.color.google_logo_tint_color);
        if (defaultLogo == null || sDefaultLogoTint != tint) {
            final Resources resources = context.getResources();
            if (tint == Color.TRANSPARENT) {
                defaultLogo = BitmapFactory.decodeResource(resources, R.drawable.google_logo);
            } else {
                // Apply color filter on a bitmap, which will cause some performance overhead, but
                // it is worth the APK space savings by avoiding adding another large asset for the
                // logo in night mode. Not using vector drawable here because it is close to the
                // maximum recommended vector drawable size 200dpx200dp.
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inMutable = true;
                defaultLogo =
                        BitmapFactory.decodeResource(resources, R.drawable.google_logo, options);
                Paint paint = new Paint();
                paint.setColorFilter(new PorterDuffColorFilter(tint, PorterDuff.Mode.SRC_ATOP));
                Canvas canvas = new Canvas(defaultLogo);
                canvas.drawBitmap(defaultLogo, 0, 0, paint);
            }
            sDefaultLogo = new WeakReference<>(defaultLogo);
            sDefaultLogoTint = tint;
        }
        return defaultLogo;
    }

    /** Returns the logo model.*/
    public PropertyModel getModelForTesting() {
        return mLogoModel;
    }

    void setShouldFetchDoodleForTesting(boolean shouldFetchDoodle) {
        mShouldFetchDoodle = shouldFetchDoodle;
    }

    void setLogoDelegateForTesting(LogoDelegateImpl logoDelegate) {
        mLogoDelegate = logoDelegate;
    }

    void setHasLogoLoadedForCurrentSearchEngineForTesting(
            boolean hasLogoLoadedForCurrentSearchEngine) {
        mHasLogoLoadedForCurrentSearchEngine = hasLogoLoadedForCurrentSearchEngine;
    }

    boolean getIsLoadPendingForTesting() {
        return mIsLoadPending;
    }

    LogoDelegateImpl getLogoDelegateForTesting() {
        return mLogoDelegate;
    }
}
