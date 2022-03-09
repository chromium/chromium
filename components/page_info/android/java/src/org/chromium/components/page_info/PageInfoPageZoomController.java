// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;

/**
 * Controller to handle creation of {@link PageInfoPageZoomView}.
 */
public class PageInfoPageZoomController implements PageInfoSubpageController {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageZoomControllerObserver mObserver;
    private final PageInfoPageZoomView.PageZoomViewDelegate mViewDelegate;

    private final String mTitle;
    private final String mSubpageTitle;

    private WebContents mWebContents;
    private PageInfoPageZoomView mSubPage;

    /**
     * Observer used to give main controller signals to change the dialog dim amount.
     */
    interface PageZoomControllerObserver {
        void onSubpageCreated();
        void onSubpageRemoved();
    }

    public PageInfoPageZoomController(PageInfoMainController mainController,
            PageInfoRowView pageZoomRowView, WebContents webContents,
            PageZoomControllerObserver observer) {
        mMainController = mainController;
        mRowView = pageZoomRowView;
        mObserver = observer;

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

        mViewDelegate = new PageInfoPageZoomView.PageZoomViewDelegate() {
            @Override
            public void setZoomLevel(double newZoomLevel) {
                HostZoomMap.setZoomLevel(mWebContents, newZoomLevel);
            }

            @Override
            public double getZoomLevel() {
                return HostZoomMap.getZoomLevel(mWebContents);
            }
        };
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
        mObserver.onSubpageCreated();
        mSubPage = new PageInfoPageZoomView(mRowView.getContext(), mViewDelegate);
        return mSubPage.getMainView();
    }

    @Override
    public void onSubpageRemoved() {
        assert mSubPage != null;
        mSubPage = null;
        mWebContents = null;
        mObserver.onSubpageRemoved();
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onNativeInitialized() {}
}