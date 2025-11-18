// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.prefs.PrefService;

/** Preferences that allows the user to configure address bar. */
@NullMarked
public class AddressBarPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private static final int HIGHLIGHT_ANIMATION_DELAY_MS = 3000;
    private static final int HIGHLIGHT_ANIMATION_DURATION_MS = 1000;

    private RadioButtonWithDescriptionLayout mGroup;
    private RadioButtonWithDescription mTopButton;
    private RadioButtonWithDescription mBottomButton;
    private @HighlightedOption int mHighlightedOption;

    // We choose this as a "default return value" because top omnibox is the default today. Choose
    // TOP_SETTINGS because TOP_LONG_PRESS causes an animation (see ToolbarPositionController).
    private static final @ToolbarPositionAndSource int DEFAULT_POSITION_AND_SOURCE =
            ToolbarPositionAndSource.TOP_SETTINGS;

    public AddressBarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.address_bar_preference);
    }

    /**
     * Set Address Bar highlighted option. Called before onBindViewHolder.
     *
     * @param highlightedOption The highlighted option for this preference.
     */
    public void init(@HighlightedOption int highlightedOption) {
        mHighlightedOption = highlightedOption;
    }

    /** If native is initialized, writes the toolbar position and source. */
    public static void setToolbarPositionAndSource(@ToolbarPositionAndSource int sharedPref) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        @Nullable PrefService localPrefService = LocalStatePrefs.get();
        if (localPrefService != null) {
            // Set the local pref service value first, since this is the source of truth.
            localPrefService.setBoolean(
                    Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, isToolbarAtBottom(sharedPref));
        }
        sharedPreferencesManager.writeInt(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, sharedPref);
    }

    /**
     * Returns whether the toolbar is user-configured to show on top. If no value has been set
     * explicitly by the user a default param is used. The value of the default param is
     * configurable for experimental purposes but defaults to top.
     */
    public static boolean isToolbarConfiguredToShowOnTop() {
        // enumToBoolean is true if position is bottom, but this method wants whether it's on top.
        var t = computeToolbarPositionAndSource();
        return !isToolbarAtBottom(t);
    }

    /** Returns enum representing toolbar position and source (why the toolbar position was set.) */
    public static @ToolbarPositionAndSource int computeToolbarPositionAndSource() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        @ToolbarPositionAndSource int existingPositionAndSource = computeToolbarTopAnchoredPref();
        boolean isChromeSharedPrefSet =
                existingPositionAndSource != ToolbarPositionAndSource.UNDEFINED;
        boolean existingPositionAsBool = isToolbarAtBottom(existingPositionAndSource);

        @Nullable PrefService localPrefService = LocalStatePrefs.get();
        if (localPrefService == null) return existingPositionAndSource;
        boolean isLocalStateSet = localPrefService.hasPrefPath(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION);
        boolean existingLocalState =
                localPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION);

        // Chrome shared pref is set, local state is not
        if (isChromeSharedPrefSet && !isLocalStateSet) {
            localPrefService.setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, existingPositionAsBool);
            return existingPositionAndSource;
        }
        // Chrome pref is not set and local state is set.
        if (!isChromeSharedPrefSet && isLocalStateSet) {
            int positionAndSource = fromLocalState(existingLocalState);
            sharedPreferencesManager.writeInt(
                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, positionAndSource);
            return positionAndSource;
        }
        // Chrome shared pref and local state are both set, and ...
        if (isChromeSharedPrefSet && isLocalStateSet) {
            // ... the prefs agree:
            if (existingLocalState == existingPositionAsBool) {
                return existingPositionAndSource;
            }
            // ... the prefs don't agree. Use local state as source of truth:
            @ToolbarPositionAndSource int positionAndSource = fromLocalState(existingLocalState);
            sharedPreferencesManager.writeInt(
                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, positionAndSource);
        }
        // Neither is set.
        return DEFAULT_POSITION_AND_SOURCE;
    }

    private static @ToolbarPositionAndSource int computeToolbarTopAnchoredPref() {
        try {
            Boolean oldPrefValue =
                    ChromeSharedPreferences.getInstance()
                            .readBoolean(
                                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                    ChromeFeatureList.sAndroidBottomToolbarDefaultToTop.getValue());
            // When transitioning to the new preference key value, use settings as the source to
            // prevent an animation. The first time this function gets called is during startup,
            // and the toolbar will appear buggy if the position transition has an animation.
            if (ChromeSharedPreferences.getInstance()
                    .contains(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED)) {
                if (oldPrefValue) {
                    ChromeSharedPreferences.getInstance()
                            .writeInt(
                                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                    ToolbarPositionAndSource.TOP_SETTINGS);
                    return ToolbarPositionAndSource.TOP_SETTINGS;
                } else {
                    ChromeSharedPreferences.getInstance()
                            .writeInt(
                                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                    ToolbarPositionAndSource.BOTTOM_SETTINGS);
                    return ToolbarPositionAndSource.BOTTOM_SETTINGS;
                }
            }
        } catch (ClassCastException e) {
        }
        @ToolbarPositionAndSource
        int posAndSource =
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                ToolbarPositionAndSource.UNDEFINED);
        if (posAndSource == ToolbarPositionAndSource.UNDEFINED) {
            return fromLocalState(!ChromeFeatureList.sAndroidBottomToolbarDefaultToTop.getValue());
        }
        return posAndSource;
    }

    @Override
    @Initializer
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mGroup =
                (RadioButtonWithDescriptionLayout)
                        holder.findViewById(R.id.address_bar_radio_group);
        mGroup.setOnCheckedChangeListener(this);

        mTopButton = (RadioButtonWithDescription) holder.findViewById(R.id.address_bar_top);
        mBottomButton = (RadioButtonWithDescription) holder.findViewById(R.id.address_bar_bottom);

        if (mHighlightedOption == HighlightedOption.BOTTOM_TOOLBAR) {
            int highlightColor = SemanticColorUtils.getSettingsBackgroundColor(getContext());
            int defaultColor = SemanticColorUtils.getDefaultBgColor(getContext());
            mBottomButton.setBackgroundColor(highlightColor);

            // Add a post delayed task to make the highlight fade out after a period of time.
            new Handler()
                    .postDelayed(
                            () -> {
                                if (!mBottomButton.isAttachedToWindow()) return;

                                ValueAnimator colorAnimation =
                                        ValueAnimator.ofObject(
                                                new ArgbEvaluator(), highlightColor, defaultColor);
                                colorAnimation.setDuration(HIGHLIGHT_ANIMATION_DURATION_MS);
                                colorAnimation.addUpdateListener(
                                        animator ->
                                                mBottomButton.setBackgroundColor(
                                                        (int) animator.getAnimatedValue()));
                                colorAnimation.start();
                            },
                            HIGHLIGHT_ANIMATION_DELAY_MS);
        }

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            // TODO(crbug.com/439911511): Set the value directly in the layout instead.
            int verticalPadding =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.settings_item_default_padding);
            mTopButton.setPadding(
                    mTopButton.getPaddingLeft(),
                    verticalPadding,
                    mTopButton.getPaddingRight(),
                    verticalPadding);
            mBottomButton.setPadding(
                    mBottomButton.getPaddingLeft(),
                    verticalPadding,
                    mBottomButton.getPaddingRight(),
                    verticalPadding);
        }

        initializeRadioButtonSelection();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        boolean isTop = mTopButton.isChecked();
        if (isTop) {
            setToolbarPositionAndSource(ToolbarPositionAndSource.TOP_SETTINGS);
        } else {
            setToolbarPositionAndSource(ToolbarPositionAndSource.BOTTOM_SETTINGS);
        }
    }

    private void initializeRadioButtonSelection() {
        boolean showOnTop = isToolbarConfiguredToShowOnTop();
        mTopButton.setChecked(showOnTop);
        mBottomButton.setChecked(!showOnTop);
    }

    @VisibleForTesting
    RadioButtonWithDescription getTopRadioButton() {
        return mTopButton;
    }

    @VisibleForTesting
    RadioButtonWithDescription getBottomRadioButton() {
        return mBottomButton;
    }

    private static boolean isToolbarAtBottom(@ToolbarPositionAndSource int pref) {
        return (pref == ToolbarPositionAndSource.BOTTOM_LONG_PRESS
                || pref == ToolbarPositionAndSource.BOTTOM_SETTINGS);
    }

    private static @ToolbarPositionAndSource int fromLocalState(boolean isInBottomPosition) {
        if (isInBottomPosition) return ToolbarPositionAndSource.BOTTOM_SETTINGS;
        return DEFAULT_POSITION_AND_SOURCE;
    }
}
