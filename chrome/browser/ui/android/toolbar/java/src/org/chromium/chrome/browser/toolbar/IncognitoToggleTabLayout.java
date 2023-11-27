// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.widget.ChromeImageView;

/** TabLayout shown in the Horizontal Tab Switcher. */
public class IncognitoToggleTabLayout extends TabLayout {
    private final TabLayout.Tab mStandardButton;
    private final TabLayout.Tab mIncognitoButton;
    private final ImageView mStandardButtonIcon;
    private final ImageView mIncognitoButtonIcon;
    private final TabSwitcherDrawable mTabSwitcherDrawable;

    private final ColorStateList mTabIconDarkColor;
    private final ColorStateList mTabIconLightColor;
    private final ColorStateList mTabIconSelectedDarkColor;
    private final ColorStateList mIncognitoSelectedColor;

    private final Callback<Integer> mNormalTabCountSupplierObserver;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    /** Constructor for inflating from XML. */
    public IncognitoToggleTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTabIconDarkColor =
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_tint_list);
        mTabIconSelectedDarkColor =
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_accent1_tint_list);
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mIncognitoSelectedColor =
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_control_color_active_dark);

        mStandardButtonIcon = new ChromeImageView(getContext());
        mTabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
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

        addOnTabSelectedListener(
                new TabLayout.OnTabSelectedListener() {
                    @Override
                    public void onTabSelected(TabLayout.Tab tab) {
                        setSelectedModel(mIncognitoButton.isSelected());
                    }

                    @Override
                    public void onTabUnselected(TabLayout.Tab tab) {}

                    @Override
                    public void onTabReselected(TabLayout.Tab tab) {}
                });

        mNormalTabCountSupplierObserver =
                (tabCount) -> {
                    mTabSwitcherDrawable.updateForTabCount(tabCount, /* incognito= */ false);
                };
    }

    /**
     * @param selector A {@link TabModelSelector} to provide information about open tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mTabModelSelector == null) return;
        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        setStateBasedOnModel();
                    }

                    @Override
                    public void onTabStateInitialized() {
                        attachTabCountSupplierObserver();
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        setStateBasedOnModel();

        if (mTabModelSelector.isTabStateInitialized()) {
            attachTabCountSupplierObserver();
        }
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            TabModel model = mTabModelSelector.getModel(false);
            // If the tab model is already destroyed the TabModelSelector only has an EmptyTabModel.
            // Skip removing the observer as otherwise it will assert.
            if (!(model instanceof EmptyTabModel)) {
                model.getTabCountSupplier().removeObserver(mNormalTabCountSupplierObserver);
            }
        }
    }

    private void setStateBasedOnModel() {
        if (mTabModelSelector == null) return;
        final boolean isIncognitoSelected = mTabModelSelector.isIncognitoSelected();

        // Update the selected tab indicator
        setSelectedTabIndicatorColor(
                isIncognitoSelected
                        ? mIncognitoSelectedColor.getDefaultColor()
                        : mTabIconSelectedDarkColor.getDefaultColor());

        // Update the Tab Switcher (Standard button) tab
        mTabSwitcherDrawable.setTint(
                isIncognitoSelected ? mTabIconLightColor : mTabIconSelectedDarkColor);
        ImageViewCompat.setImageTintList(
                mStandardButtonIcon,
                isIncognitoSelected ? mTabIconLightColor : mTabIconSelectedDarkColor);

        // Update the Incognito tab
        ImageViewCompat.setImageTintList(
                mIncognitoButtonIcon,
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
            Integer tabCount = mTabModelSelector.getCurrentModelTabCountSupplier().get();
            RecordHistogram.recordBooleanHistogram(
                    "Android.TabSwitcher.IncognitoClickedIsEmpty",
                    tabCount == null ? true : tabCount.intValue() == 0);
        }

        final int stackAnnouncementId =
                incognitoSelected
                        ? R.string.accessibility_tab_switcher_incognito_stack_selected
                        : R.string.accessibility_tab_switcher_standard_stack_selected;
        announceForAccessibility(getResources().getString(stackAnnouncementId));
    }

    private void attachTabCountSupplierObserver() {
        mTabModelSelector
                .getModel(false)
                .getTabCountSupplier()
                .addObserver(mNormalTabCountSupplierObserver);
    }
}
