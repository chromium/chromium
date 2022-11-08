// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.widget.ChromeImageView;

/**
 * TabLayout shown in the Horizontal Tab Switcher.
 */
public class IncognitoToggleTabLayout extends TabLayout implements TabCountObserver {
    private final TabLayout.Tab mStandardButton;
    private final TabLayout.Tab mIncognitoButton;
    private final ImageView mStandardButtonIcon;
    private final ImageView mIncognitoButtonIcon;
    private final TabSwitcherDrawable mTabSwitcherDrawable;

    private final ColorStateList mTabIconDarkColor;
    private final ColorStateList mTabIconLightColor;
    private final ColorStateList mTabIconSelectedDarkColor;
    private final ColorStateList mIncognitoSelectedColor;

    private TabModelSelector mTabModelSelector;
    private TabCountProvider mTabCountProvider;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    /**
     * Constructor for inflating from XML.
     */
    public IncognitoToggleTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTabIconDarkColor = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_tint_list);
        mTabIconSelectedDarkColor = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_accent1_tint_list);
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mIncognitoSelectedColor = AppCompatResources.getColorStateList(
                getContext(), R.color.default_control_color_active_dark);

        mStandardButtonIcon = new ChromeImageView(getContext());
        mTabSwitcherDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(
                getContext(), BrandedColorScheme.APP_DEFAULT);
        mStandardButtonIcon.setImageDrawable(mTabSwitcherDrawable);
        mStandardButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_standard_stack));
        mIncognitoButtonIcon = new ChromeImageView(getContext());
        mIncognitoButtonIcon.setImageResource(R.drawable.incognito_small);
        mIncognitoButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_incognito_stack));

        mStandardButton = newTab().setCustomView(mStandardButtonIcon);
        addTab(mStandardButton);
        mIncognitoButton = newTab().setCustomView(mIncognitoButtonIcon);
        addTab(mIncognitoButton);

        addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                setSelectedModel(mIncognitoButton.isSelected());
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });
    }

    /**
     * @param selector A {@link TabModelSelector} to provide information about open tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mTabModelSelector == null) return;
        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                setStateBasedOnModel();
            }

            @Override
            public void onTabStateInitialized() {
                updateTabSwitcherDrawableCount();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        setStateBasedOnModel();

        if (mTabModelSelector.isTabStateInitialized()) {
            updateTabSwitcherDrawableCount();
        }
    }

    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountProvider.addObserverAndTrigger(this);
    }

    /**
     * Update the visual state based on number of normal (non-incognito) tabs present.
     * @param tabCount The number of normal tabs.
     */
    @Override
    public void onTabCountChanged(int tabCount, boolean isIncognito) {
        if (!isIncognito) mTabSwitcherDrawable.updateForTabCount(tabCount, isIncognito);
    }

    public void destroy() {
        if (mTabModelSelector != null) mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        if (mTabCountProvider != null) mTabCountProvider.removeObserver(this);
    }

    private void setStateBasedOnModel() {
        if (mTabModelSelector == null) return;
        final boolean isIncognitoSelected = mTabModelSelector.isIncognitoSelected();

        // Update the selected tab indicator
        setSelectedTabIndicatorColor(isIncognitoSelected
                        ? mIncognitoSelectedColor.getDefaultColor()
                        : mTabIconSelectedDarkColor.getDefaultColor());

        // Update the Tab Switcher (Standard button) tab
        mTabSwitcherDrawable.setTint(
                isIncognitoSelected ? mTabIconLightColor : mTabIconSelectedDarkColor);
        ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon,
                isIncognitoSelected ? mTabIconLightColor : mTabIconSelectedDarkColor);

        // Update the Incognito tab
        ApiCompatibilityUtils.setImageTintList(mIncognitoButtonIcon,
                isIncognitoSelected ? mIncognitoSelectedColor : mTabIconDarkColor);

        // Ensure the tab in tab layout is correctly selected when tab switcher is
        // first opened.
        if (isIncognitoSelected && !mIncognitoButton.isSelected()) {
            mIncognitoButton.select();
        } else if (!isIncognitoSelected && !mStandardButton.isSelected()) {
            mStandardButton.select();
        }
    }

    private void setSelectedModel(boolean incognitoSelected) {
        if (mTabModelSelector == null
                || incognitoSelected == mTabModelSelector.isIncognitoSelected()) {
            return;
        }

        mTabModelSelector.commitAllTabClosures();
        mTabModelSelector.selectModel(incognitoSelected);

        if (incognitoSelected) {
            RecordHistogram.recordBooleanHistogram("Android.TabSwitcher.IncognitoClickedIsEmpty",
                    mTabCountProvider.getTabCount() == 0);
        }

        final int stackAnnouncementId = incognitoSelected
                ? R.string.accessibility_tab_switcher_incognito_stack_selected
                : R.string.accessibility_tab_switcher_standard_stack_selected;
        announceForAccessibility(getResources().getString(stackAnnouncementId));
    }

    private void updateTabSwitcherDrawableCount() {
        final int tabCount = mTabModelSelector.getTabModelFilterProvider()
                                     .getTabModelFilter(
                                             /*isIncognito=*/false)
                                     .getTotalTabCount();
        mTabSwitcherDrawable.updateForTabCount(tabCount, false);
    }
}
