// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorRes;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;

/** Class for controlling the page info connection section. */
public class PageInfoConnectionController
        implements PageInfoSubpageController, ConnectionInfoView.ConnectionInfoDelegate {
    private PageInfoMainController mMainController;
    private final WebContents mWebContents;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mContentPublisher;
    private final boolean mIsInternalPage;
    private String mTitle;
    private ConnectionInfoView mInfoView;
    private ViewGroup mContainer;

    public PageInfoConnectionController(
            PageInfoMainController mainController,
            PageInfoRowView view,
            WebContents webContents,
            PageInfoControllerDelegate delegate,
            String publisher,
            boolean isInternalPage) {
        mMainController = mainController;
        mRowView = view;
        mWebContents = webContents;
        mDelegate = delegate;
        mContentPublisher = publisher;
        mIsInternalPage = isInternalPage;
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
        mInfoView = ConnectionInfoView.create(mRowView.getContext(), mWebContents, this);
        return mContainer;
    }

    @Override
    public void onSubpageRemoved() {
        mContainer = null;
        mInfoView.onDismiss();
    }

    private static @ColorRes int getSecurityIconColor(@ConnectionSecurityLevel int securityLevel) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.DANGEROUS:
                return R.color.default_text_color_error;
            case ConnectionSecurityLevel.WARNING:
                return R.color.default_text_color_error;
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
                return 0;
            default:
                assert false;
                return 0;
        }
    }

    /** Whether to show a 'Details' link to the connection info popup. */
    private boolean isConnectionDetailsLinkVisible() {
        // If Paint Preview is being shown, it completely obstructs the WebContents and users
        // cannot interact with it. Hence, showing connection details is not relevant.
        return mContentPublisher == null
                && !mDelegate.isShowingOfflinePage()
                && !mDelegate.isShowingPaintPreviewPage()
                && mDelegate.getPdfPageType() == 0
                && !mIsInternalPage;
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    public void setSecurityDescription(String summary, String details) {
        // Display the appropriate connection message.
        SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
        CharSequence title = null;
        CharSequence subtitle = null;
        boolean hasClickCallback;

        assert mRowView.getContext() != null;
        if (mContentPublisher != null) {
            messageBuilder.append(
                    mRowView.getContext()
                            .getString(R.string.page_info_domain_hidden, mContentPublisher));
        } else if (mDelegate.isShowingPaintPreviewPage()) {
            messageBuilder.append(mDelegate.getPaintPreviewPageConnectionMessage());
        } else if (mDelegate.getOfflinePageConnectionMessage() != null) {
            messageBuilder.append(mDelegate.getOfflinePageConnectionMessage());
        } else if (mDelegate.getPdfPageType() != 0) {
            messageBuilder.append(mDelegate.getPdfPageConnectionMessage());
        } else {
            if (!summary.isEmpty()) {
                title = summary;
            }
            messageBuilder.append(details);
        }

        if (isConnectionDetailsLinkVisible() && messageBuilder.length() > 0) {
            messageBuilder.append(" ");
            SpannableString detailsText =
                    new SpannableString(mRowView.getContext().getString(R.string.details_link));
            final ForegroundColorSpan blueSpan =
                    new ForegroundColorSpan(
                            SemanticColorUtils.getDefaultTextColorLink(mRowView.getContext()));
            detailsText.setSpan(
                    blueSpan, 0, detailsText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            messageBuilder.append(detailsText);
        }

        // When a preview is being shown for a secure page, the security message is not shown. Thus,
        // messageBuilder maybe empty.
        if (messageBuilder.length() > 0) {
            subtitle = messageBuilder;
        }
        hasClickCallback = isConnectionDetailsLinkVisible();

        setConnectionInfo(title, subtitle, hasClickCallback);
    }

    private void setConnectionInfo(
            CharSequence title, CharSequence subtitle, boolean hasClickCallback) {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        mTitle = title != null ? title.toString() : null;
        rowParams.title = mTitle;
        rowParams.subtitle = subtitle;
        rowParams.visible = rowParams.title != null || rowParams.subtitle != null;
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
        // Page info should always show lock icon as the connection security indicator.
        rowParams.iconResId =
                SecurityStatusIcon.getSecurityIconResource(
                        securityLevel,
                        /* isSmallDevice= */ false,
                        /* skipIconForNeutralState= */ false,
                        /* useUpdatedConnectionSecurityIndicators= */ false);
        rowParams.iconTint = getSecurityIconColor(securityLevel);
        if (hasClickCallback) rowParams.clickCallback = this::launchSubpage;
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

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}
}
