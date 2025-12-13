// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RichRadioButtonData;
import org.chromium.components.browser_ui.widget.RichRadioButtonList;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

/**
 * Controller for the location precision chooser UI (radio buttons). Responsible for creating,
 * displaying, and managing the state of the precision selection.
 */
@NullMarked
public class LocationPrecisionChooserController {

    private int mArm = ApproximateGeolocationPromptArm.NO_ARM_SELECTED;

    private final Context mContext;
    private final LinearLayout mContainer;
    private final @LocationAccuracy int mInitialSelection;
    private final @Nullable Consumer<Integer> mSelectionListener;
    private final List<RichRadioButtonData> mOptionsToDisplay;
    private final Map<String, Integer> mIdToAccuracyMap;

    private @Nullable RichRadioButtonList mRichRadioButtonList;

    public LocationPrecisionChooserController(
            Context context,
            LinearLayout container,
            @LocationAccuracy int initialSelection,
            @Nullable Consumer<Integer> selectionListener) {

        mContext = context;
        mContainer = container;
        mInitialSelection = initialSelection;
        mSelectionListener = selectionListener;

        mArm =
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PROMPT_ARM.getValue()
                                == ApproximateGeolocationPromptArm.NO_ARM_SELECTED
                        ? ApproximateGeolocationPromptArm.ARM_1
                        : PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PROMPT_ARM
                                .getValue();

        mIdToAccuracyMap = new HashMap<>();
        mOptionsToDisplay = buildRichRadioButtonOptions();
    }

    public void show() {
        if (mContainer == null || mOptionsToDisplay == null || mOptionsToDisplay.isEmpty()) {
            hide();
            return;
        }

        mContainer.removeAllViews();
        mContainer.setVisibility(View.VISIBLE);

        mRichRadioButtonList = new RichRadioButtonList(mContext);
        mRichRadioButtonList.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        @RichRadioButtonList.LayoutMode int currentLayoutMode = getLayoutMode();
        mRichRadioButtonList.initialize(
                mOptionsToDisplay,
                currentLayoutMode,
                (selectedId) -> {
                    if (mSelectionListener != null) {
                        @LocationAccuracy Integer accuracy = mIdToAccuracyMap.get(selectedId);
                        if (accuracy != null) {
                            mSelectionListener.accept(accuracy);
                        }
                    }
                });

        String initialSelectionId = accuracyToId(mInitialSelection);
        if (initialSelectionId != null) {
            mRichRadioButtonList.setSelectedItem(initialSelectionId);
        }

        mContainer.addView(mRichRadioButtonList);
    }

    public void hide() {
        if (mContainer != null) {
            mContainer.setVisibility(View.GONE);
            mContainer.removeAllViews();
            mRichRadioButtonList = null;
        }
    }

    /**
     * Returns the overall layout mode for the RichRadioButtonList based on the experiment arm.
     *
     * @return The layout mode for the RichRadioButtonList.
     */
    private @RichRadioButtonList.LayoutMode int getLayoutMode() {
        @RichRadioButtonList.LayoutMode
        int currentLayoutMode = RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN;

        if (mArm == ApproximateGeolocationPromptArm.ARM_1
                || mArm == ApproximateGeolocationPromptArm.ARM_2
                || mArm == ApproximateGeolocationPromptArm.ARM_3
                || mArm == ApproximateGeolocationPromptArm.ARM_6) {
            currentLayoutMode = RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN;
        } else if (mArm == ApproximateGeolocationPromptArm.ARM_4
                || mArm == ApproximateGeolocationPromptArm.ARM_5) {
            currentLayoutMode = RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID;
        }

        return currentLayoutMode;
    }

    /**
     * Builds the list of {@link RichRadioButtonData} options for the chooser. The internal
     * vertical/horizontal layout of each RichRadioButton will be determined by RichRadioButtonList
     * based on the overall LayoutMode (single column vs. grid).
     */
    private List<RichRadioButtonData> buildRichRadioButtonOptions() {
        List<RichRadioButtonData> options = new ArrayList<>();

        final String preciseId = "precise_location_option";
        final String approximateId = "approximate_location_option";

        mIdToAccuracyMap.put(preciseId, LocationAccuracy.PRECISE);
        mIdToAccuracyMap.put(approximateId, LocationAccuracy.APPROXIMATE);

        RichRadioButtonData.Builder preciseOptionBuilder =
                new RichRadioButtonData.Builder(
                        preciseId, mContext.getString(R.string.permission_allow_precise_geo));
        RichRadioButtonData.Builder approximateOptionBuilder =
                new RichRadioButtonData.Builder(
                        approximateId,
                        mContext.getString(R.string.permission_allow_approximate_geo));

        switch (mArm) {
            case ApproximateGeolocationPromptArm.ARM_1:
                break;
            case ApproximateGeolocationPromptArm.ARM_2:
            case ApproximateGeolocationPromptArm.ARM_4:
                preciseOptionBuilder.setIconResId(R.drawable.location_precise);
                approximateOptionBuilder.setIconResId(R.drawable.location_approximate);
                break;

            case ApproximateGeolocationPromptArm.ARM_3:
            case ApproximateGeolocationPromptArm.ARM_5:
                preciseOptionBuilder
                        .setIconResId(R.drawable.location_precise)
                        .setDescription(
                                mContext.getString(
                                        R.string.permission_allow_precise_geo_description));
                approximateOptionBuilder
                        .setIconResId(R.drawable.location_approximate)
                        .setDescription(
                                mContext.getString(
                                        R.string.permission_allow_approximate_geo_description));
                break;

            case ApproximateGeolocationPromptArm.ARM_6:
                preciseOptionBuilder
                        .setIconResId(R.drawable.location_precise)
                        .setDescription(
                                mContext.getString(
                                        R.string.permission_allow_precise_geo_long_description));
                approximateOptionBuilder
                        .setIconResId(R.drawable.location_approximate)
                        .setDescription(
                                mContext.getString(
                                        R.string
                                                .permission_allow_approximate_geo_long_description));
                break;

            default:
                break;
        }

        options.add(preciseOptionBuilder.build());
        options.add(approximateOptionBuilder.build());
        return options;
    }

    private @Nullable String accuracyToId(@LocationAccuracy int accuracy) {
        for (Map.Entry<String, Integer> entry : mIdToAccuracyMap.entrySet()) {
            if (entry.getValue().equals(accuracy)) {
                return entry.getKey();
            }
        }
        return null;
    }
}
