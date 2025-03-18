// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Class for controlling the page info ad personalization section. */
@NullMarked
public class PageInfoAdPersonalizationController extends PageInfoPreferenceSubpageController {
    public static final int ROW_ID = View.generateViewId();
    private static @Nullable List<String> sTopicsForTesting;

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private @Nullable PageInfoAdPersonalizationSettings mSubPage;

    private boolean mHasJoinedUserToInterestGroup;
    private @Nullable List<String> mTopics;

    public PageInfoAdPersonalizationController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate) {
        super(delegate);
        mMainController = mainController;
        mRowView = rowView;
    }

    public void setAdPersonalizationInfo(
            boolean hasJoinedUserToInterestGroup, List<String> topics) {
        mHasJoinedUserToInterestGroup = hasJoinedUserToInterestGroup;
        mTopics = topics;
        if (mTopics.isEmpty() && sTopicsForTesting != null) {
            mTopics = sTopicsForTesting;
        }
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = hasJoinedUserToInterestGroup || !mTopics.isEmpty();
        rowParams.title = getSubpageTitle();
        rowParams.iconResId = R.drawable.gm_ads_click_24;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mRowView.getContext().getString(R.string.page_info_ad_privacy_header);
    }

    @Override
    public @Nullable View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        mSubPage = new PageInfoAdPersonalizationSettings();
        PageInfoAdPersonalizationSettings.Params params =
                new PageInfoAdPersonalizationSettings.Params(
                        /* hasJoinedUserToInterestGroup= */ mHasJoinedUserToInterestGroup,
                        /* topicInfo= */ assertNonNull(mTopics),
                        /* onManageInterestsButtonClicked= */ () -> {
                            mMainController.recordAction(
                                    PageInfoAction.PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED);
                            getDelegate().showAdPersonalizationSettings();
                        });
        mSubPage.setParams(params);
        return addSubpageFragment(mSubPage);
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        removeSubpageFragment();
        mSubPage = null;
    }

    public static void setTopicsForTesting(List<String> topics) {
        sTopicsForTesting = topics;
        ResettersForTesting.register(() -> sTopicsForTesting = null);
    }
}
