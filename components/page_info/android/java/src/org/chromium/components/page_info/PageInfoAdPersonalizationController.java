// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Class for controlling the page info ad personalization section.
 */
public class PageInfoAdPersonalizationController extends PageInfoPreferenceSubpageController {
    public static final int ROW_ID = View.generateViewId();

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private PageInfoAdPersonalizationPreference mSubPage;

    private List<String> mInfo;

    public PageInfoAdPersonalizationController(PageInfoMainController mainController,
            PageInfoRowView rowView, PageInfoControllerDelegate delegate) {
        super(delegate);
        mMainController = mainController;
        mRowView = rowView;

        fetchAdPersonalizationInfo();

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = !mInfo.isEmpty();
        rowParams.title = getSubpageTitle();
        rowParams.iconResId = R.drawable.gm_ads_click_24;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    private void fetchAdPersonalizationInfo() {
        // TODO(crbug.com/1286276): Populate with real data from site.
        if (mMainController.getURL().domainIs("example.com")) {
            mInfo = Arrays.asList("Arts & entertainment");
        } else {
            mInfo = Collections.emptyList();
        }
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mRowView.getContext().getResources().getString(
                R.string.page_info_ad_personalization_title);
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        mSubPage = new PageInfoAdPersonalizationPreference();
        PageInfoAdPersonalizationPreference.Params params =
                new PageInfoAdPersonalizationPreference.Params();
        params.topicInfo = mInfo;
        params.onManageInterestsButtonClicked = getDelegate()::showAdPersonalizationSettings;
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
}
