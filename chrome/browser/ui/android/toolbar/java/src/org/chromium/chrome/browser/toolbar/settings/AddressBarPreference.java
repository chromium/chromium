// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/** Preferences that allows the user to configure address bar. */
@NullMarked
public class AddressBarPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescriptionLayout mGroup;
    private RadioButtonWithDescription mTopButton;
    private RadioButtonWithDescription mBottomButton;

    public AddressBarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.address_bar_preference);
    }

    /**
     * Returns whether the toolbar is user-configured to show on top. If no value has been set
     * explicitly by the user a default param is used. The value of the default param is
     * configurable for experimental purposes but defaults to top.
     */
    public static boolean isToolbarConfiguredToShowOnTop() {
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
                } else {
                    ChromeSharedPreferences.getInstance()
                            .writeInt(
                                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                    ToolbarPositionAndSource.BOTTOM_SETTINGS);
                }
            }
            return oldPrefValue;
        } catch (ClassCastException e) {
            int prefValue =
                    ChromeSharedPreferences.getInstance()
                            .readInt(
                                    ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                                    ToolbarPositionAndSource.UNDEFINED);
            return switch (prefValue) {
                case ToolbarPositionAndSource.TOP_LONG_PRESS,
                        ToolbarPositionAndSource.TOP_SETTINGS -> true;
                case ToolbarPositionAndSource.BOTTOM_LONG_PRESS,
                        ToolbarPositionAndSource.BOTTOM_SETTINGS -> false;
                default -> ChromeFeatureList.sAndroidBottomToolbarDefaultToTop.getValue();
            };
        }
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

        initializeRadioButtonSelection();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        boolean isTop = mTopButton.isChecked();
        if (isTop) {
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                            ToolbarPositionAndSource.TOP_SETTINGS);
        } else {
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                            ToolbarPositionAndSource.BOTTOM_SETTINGS);
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
}
