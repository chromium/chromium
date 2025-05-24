// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** The container of the two home page buttons. */
@NullMarked
public class HomePageButtonsContainerView extends LinearLayout {
    private List<HomePageButtonView> mHomePageButtonsList;

    public HomePageButtonsContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        HomePageButtonView homePageButton = findViewById(R.id.home_button);
        HomePageButtonView ntpCustomizationButton = findViewById(R.id.ntp_customization_button);
        mHomePageButtonsList =
                Collections.unmodifiableList(Arrays.asList(homePageButton, ntpCustomizationButton));
    }

    void setButtonVisibility(int buttonIndex, boolean visible) {
        getButtonByIndex(buttonIndex).setVisibility(visible);
    }

    void updateButtonData(int buttonIndex, HomePageButtonData homePageButtonData) {
        getButtonByIndex(buttonIndex).updateButtonData(homePageButtonData);
    }

    void setColorStateList(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(getButtonByIndex(/* buttonIndex= */ 0), colorStateList);
        ImageViewCompat.setImageTintList(getButtonByIndex(/* buttonIndex= */ 1), colorStateList);
    }

    void setButtonBackgroundResource(int backgroundResource) {
        getButtonByIndex(/* buttonIndex= */ 0).setBackgroundResource(backgroundResource);
        getButtonByIndex(/* buttonIndex= */ 1).setBackgroundResource(backgroundResource);
    }

    HomePageButtonView getButtonByIndex(int buttonIndex) {
        return mHomePageButtonsList.get(buttonIndex);
    }

    void setHomePageButtonsListForTesting(List<HomePageButtonView> homePageButtonsList) {
        mHomePageButtonsList = homePageButtonsList;
    }
}
