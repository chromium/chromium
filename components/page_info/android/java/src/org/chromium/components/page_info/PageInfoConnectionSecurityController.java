// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * Controls the display of connection security information in the PageInfo UI. If the connection is
 * secure, a single row showing just the summary of the connection security description (i.e.
 * "Connection is secure") is displayed and it functions as a button to show a
 * ConnectionSecurityView as a PageInfo subpage. If the connection is not secure, the
 * ConnectionSecurityView is shown directly in the PageInfo UI.
 */
@NullMarked
public class PageInfoConnectionSecurityController implements PageInfoSubpageController {
    private final PageInfoMainController mMainController;
    private final WebContents mWebContents;
    private final ConnectionSecurityView mView;
    private final PageInfoRowView mRowView;
    private @Nullable ConnectionSecurityView mActiveView;
    private final ConnectionSecurityView.ViewParams mViewParams;
    private final long mNativeConnectionSecurityController;

    public PageInfoConnectionSecurityController(
            PageInfoMainController mainController,
            ConnectionSecurityView view,
            PageInfoRowView rowView,
            WebContents webContents) {
        mMainController = mainController;
        mView = view;
        mRowView = rowView;
        mWebContents = webContents;

        mViewParams = new ConnectionSecurityView.ViewParams();

        mNativeConnectionSecurityController =
                PageInfoConnectionSecurityControllerJni.get().init(this, mWebContents);
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_SECURITY_DETAILS_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public @Nullable String getSubpageTitle() {
        return mView.getContext().getString(R.string.page_info_security_subpage_header);
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        mActiveView = new ConnectionSecurityView(mView.getContext(), null);
        mActiveView.setParams(mViewParams);
        return mActiveView;
    }

    @Override
    public @Nullable View getCurrentSubpageView() {
        return mActiveView;
    }

    @Override
    public void onSubpageRemoved() {
        mActiveView = null;
    }

    private void loadIdentityInfo() {
        PageInfoConnectionSecurityControllerJni.get()
                .loadIdentityInfo(mNativeConnectionSecurityController);
    }

    /**
     * Called by PageInfoController to show a summary button in the PageInfo UI, which opens a
     * subpage displaying the full ConnectionSecurityView for the page.
     */
    public void showSecurityPageButton(String summary) {
        loadIdentityInfo();
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = summary;
        rowParams.iconResId = R.drawable.lock;
        rowParams.visible = summary != null;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    /** Called by PageInfoController to show the security info directly within the PageInfo UI. */
    public void showSecurityInfo() {
        loadIdentityInfo();
        mActiveView = mView;
        mActiveView.setParams(mViewParams);
    }

    @CalledByNative
    public void setSecurityDescription(
            int iconResId,
            int iconTint,
            String summary,
            String details,
            boolean showResetDecisionsLabel,
            byte[][] certChain,
            boolean isCert1Qwac,
            byte[][] twoQwacCertChain,
            String qwacIdentity) {
        mViewParams.iconResId = iconResId;
        mViewParams.iconTint = iconTint;
        mViewParams.summary = summary;
        mViewParams.details = details;
        if (showResetDecisionsLabel) {
            mViewParams.resetDecisionsCallback = this::resetCertDecision;
        }
        if (certChain.length != 0) {
            mViewParams.certChain = certChain;
        }
        mViewParams.isCert1Qwac = isCert1Qwac;
        if (twoQwacCertChain.length != 0) {
            mViewParams.twoQwacCertChain = twoQwacCertChain;
        }
        mViewParams.qwacIdentity = qwacIdentity;
        if (mActiveView != null) {
            mActiveView.setParams(mViewParams);
        }
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void updateSubpageIfNeeded() {}

    public void resetCertDecision() {
        PageInfoConnectionSecurityControllerJni.get()
                .resetCertDecisions(mNativeConnectionSecurityController);
        mMainController.dismiss();
    }

    public void destroy() {
        PageInfoConnectionSecurityControllerJni.get().destroy(mNativeConnectionSecurityController);
    }

    @NativeMethods
    interface Natives {
        long init(PageInfoConnectionSecurityController controller, WebContents webContents);

        void destroy(long nativeConnectionSecurityControllerAndroid);

        void loadIdentityInfo(long nativeConnectionSecurityControllerAndroid);

        void resetCertDecisions(long nativeConnectionSecurityControllerAndroid);
    }
}
