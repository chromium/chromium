// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.Toast;

/**
 * Button for creating new tabs.
 */
public class NewTabButton
        extends ChromeImageButton implements IncognitoStateObserver, View.OnLongClickListener {
    private final ColorStateList mLightModeTint;
    private final ColorStateList mDarkModeTint;
    private final boolean mIsTablet;
    private IncognitoStateProvider mIncognitoStateProvider;
    private boolean mIsIncognito;
    private boolean mIsGridTabSwitcherEnabled;
    private boolean mIsStartSurfaceEnabled;

    /**
     * Constructor for inflating from XML.
     */
    public NewTabButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        mIsIncognito = false;
        mLightModeTint = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_light_tint_list);
        mDarkModeTint = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_tint_list);
        setImageDrawable(VectorDrawableCompat.create(
                getContext().getResources(), R.drawable.new_tab_icon, getContext().getTheme()));
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
        updateDrawableTint();
        setOnLongClickListener(this);
    }

    /**
     * Set grid-type tab switcher feature flag.
     * @param isGridTabSwitcherEnabled Whether grid tab switcher is enabled.
     */
    public void setGridTabSwitcherEnabled(boolean isGridTabSwitcherEnabled) {
        if (mIsGridTabSwitcherEnabled == isGridTabSwitcherEnabled) return;
        mIsGridTabSwitcherEnabled = isGridTabSwitcherEnabled;

        updateDrawableTint();
        invalidate();
    }

    /**
     * Set start surface feature flag.
     * @param isStartSurfaceEnabled Whether start surface is enabled.
     */
    public void setStartSurfaceEnabled(boolean isStartSurfaceEnabled) {
        if (mIsStartSurfaceEnabled == isStartSurfaceEnabled) return;
        mIsStartSurfaceEnabled = isStartSurfaceEnabled;

        updateDrawableTint();
        invalidate();
    }

    @Override
    public boolean onLongClick(View v) {
        CharSequence description = getResources().getString(
                mIsIncognito ? R.string.button_new_incognito_tab : R.string.button_new_tab);
        return Toast.showAnchoredToast(getContext(), v, description);
    }

    public void setIncognitoStateProvider(IncognitoStateProvider incognitoStateProvider) {
        mIncognitoStateProvider = incognitoStateProvider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        if (mIsIncognito == isIncognito) return;
        mIsIncognito = isIncognito;

        @StringRes
        int resId = mIsIncognito ? R.string.accessibility_toolbar_btn_new_incognito_tab
                                 : R.string.accessibility_toolbar_btn_new_tab;
        setContentDescription(getResources().getText(resId));

        updateDrawableTint();
        invalidate();
    }

    /** Called when accessibility status is changed. */
    public void onAccessibilityStatusChanged() {
        updateDrawableTint();
    }

    /** Update the tint for the icon drawable for Chrome Modern. */
    private void updateDrawableTint() {
        final boolean shouldUseLightMode = mIsTablet
                || ((DeviceClassManager.enableAccessibilityLayout(getContext())
                            || mIsGridTabSwitcherEnabled || mIsStartSurfaceEnabled)
                        && mIsIncognito);
        ImageViewCompat.setImageTintList(this, shouldUseLightMode ? mLightModeTint : mDarkModeTint);
    }

    /**
     * Clean up any state when the new tab button is destroyed.
     */
    public void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
    }
}
