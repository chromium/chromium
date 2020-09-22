// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Class for controlling the page info connection section.
 */
public class PageInfoConnectionController
        implements PageInfoSubpageController, ConnectionInfoView.ConnectionInfoDelegate {
    private PageInfoMainController mMainController;
    private final WebContents mWebContents;
    private final VrHandler mVrHandler;
    private PageInfoRowView mRowView;
    private String mTitle;
    private ConnectionInfoView mInfoView;
    private ViewGroup mContainer;

    public PageInfoConnectionController(PageInfoMainController mainController, PageInfoRowView view,
            WebContents webContents, VrHandler vrHandler) {
        mMainController = mainController;
        mWebContents = webContents;
        mVrHandler = vrHandler;
        mRowView = view;
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_SECURITY_DETAILS_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        mContainer = new FrameLayout(mRowView.getContext());
        mInfoView =
                ConnectionInfoView.create(mRowView.getContext(), mWebContents, this, mVrHandler);
        return mContainer;
    }

    @Override
    public void onSubpageRemoved() {
        mContainer = null;
        mInfoView.onDismiss();
    }

    public void setConnectionInfo(PageInfoView.ConnectionInfoParams params) {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        mTitle = params.summary != null ? params.summary.toString() : null;
        rowParams.title = mTitle;
        rowParams.subtitle = params.message != null ? params.message.toString() : null;
        rowParams.visible = rowParams.title != null || rowParams.subtitle != null;
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
        rowParams.iconResId = SecurityStatusIcon.getSecurityIconResource(securityLevel,
                SecurityStateModel.shouldShowDangerTriangleForWarningLevel(),
                /*isSmallDevice=*/false,
                /*skipIconForNeutralState=*/false);
        if (params.clickCallback != null) rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    @Override
    public void onReady(ConnectionInfoView infoView) {
        if (mContainer != null) {
            mContainer.addView(infoView.getView());
        }
    }

    @Override
    public void dismiss(int actionOnContent) {
        mMainController.exitSubpage();
    }
}
