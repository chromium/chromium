// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * Controller to handle creation of {@link PageInfoPageZoomView}.
 */
public class PageInfoPageZoomController implements PageInfoSubpageController {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;

    private final WebContents mWebContents;

    private final String mTitle;
    private final String mSubpageTitle;

    private PageInfoPageZoomView mSubPage;

    public PageInfoPageZoomController(PageInfoMainController mainController,
            PageInfoRowView pageZoomRowView, WebContents webContents,
            PageInfoControllerDelegate delegate) {
        mMainController = mainController;
        mRowView = pageZoomRowView;
        mDelegate = delegate;

        mWebContents = webContents;

        mTitle = mRowView.getContext().getResources().getString(R.string.page_zoom_title);
        mSubpageTitle = mRowView.getContext().getResources().getString(R.string.page_zoom_summary);

        // Set the ViewParams for PageInfoRowView.
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = mTitle;
        rowParams.visible = true;
        rowParams.iconResId = R.drawable.ic_zoom_in;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    private void launchSubpage() {
        mMainController.launchSubpage(this);
    }

    @NonNull
    @Override
    public String getSubpageTitle() {
        return mSubpageTitle;
    }

    @Nullable
    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        mSubPage = new PageInfoPageZoomView(mRowView.getContext());
        return mSubPage.getMainView();
    }

    @Override
    public void onSubpageRemoved() {
        assert mSubPage != null;
        mSubPage = null;
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}
}