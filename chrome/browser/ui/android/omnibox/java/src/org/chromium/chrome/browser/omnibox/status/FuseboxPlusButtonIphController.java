// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Configuration;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusMediator.StatusIconType;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Controller to manage when an In-Product Help (IPH) callout bubble for the Fusebox (+) button is
 * shown.
 */
@NullMarked
class FuseboxPlusButtonIphController {
    private final UserEducationHelper mUserEducationHelper;
    private final View mStatusView;
    private final NonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private final NonNullObservableSupplier<@StatusIconType Integer> mStatusIconTypeSupplier;
    private final Callback<@FuseboxState Integer> mFuseboxStateObserver = _ -> updateVisibility();
    private final Callback<@StatusIconType Integer> mStatusIconTypeObserver =
            _ -> updateVisibility();
    private boolean mIsShowing;

    /**
     * @param userEducationHelper The {@link UserEducationHelper} to show user education events.
     * @param statusView The {@link StatusView} to anchor the IPH bubble to.
     * @param fuseboxStateSupplier Supplies the current {@link FuseboxState}.
     * @param statusIconTypeSupplier Supplies the currently resolved {@link StatusIconType}.
     */
    FuseboxPlusButtonIphController(
            UserEducationHelper userEducationHelper,
            View statusView,
            NonNullObservableSupplier<@FuseboxState Integer> fuseboxStateSupplier,
            NonNullObservableSupplier<@StatusIconType Integer> statusIconTypeSupplier) {
        mUserEducationHelper = userEducationHelper;
        mStatusView = statusView;
        mFuseboxStateSupplier = fuseboxStateSupplier;
        mStatusIconTypeSupplier = statusIconTypeSupplier;

        mFuseboxStateSupplier.addSyncObserver(mFuseboxStateObserver);
        mStatusIconTypeSupplier.addSyncObserver(mStatusIconTypeObserver);
    }

    void destroy() {
        mFuseboxStateSupplier.removeObserver(mFuseboxStateObserver);
        mStatusIconTypeSupplier.removeObserver(mStatusIconTypeObserver);
        dismiss();
    }

    private void updateVisibility() {
        // Suppress in phone landscape where the soft keyboard leaves minimal vertical space and
        // the camera cutout inset can shift the bubble arrow off-center from the (+) icon.
        boolean shouldShow =
                mFuseboxStateSupplier.get() == FuseboxState.COMPACT
                        && mStatusIconTypeSupplier.get() == StatusIconType.FUSEBOX_PLUS_BUTTON
                        && mStatusView.isShown()
                        && !isLandscapePhone();
        if (shouldShow) {
            show();
        } else {
            dismiss();
        }
    }

    private void show() {
        if (mIsShowing) return;

        mIsShowing = true;
        IphCommand command =
                new IphCommandBuilder(
                                mStatusView.getResources(),
                                FeatureConstants.FUSEBOX_PLUS_BUTTON_HINT_FEATURE,
                                /* stringId= */ R.string.fusebox_attachment_button_iph,
                                /* accessibilityStringId= */ R.string.fusebox_attachment_button_iph)
                        .setAnchorView(mStatusView)
                        .setOnDismissCallback(() -> mIsShowing = false)
                        .setOnNotShownCallback(() -> mIsShowing = false)
                        .build();

        mUserEducationHelper.requestShowIph(command);
    }

    private void dismiss() {
        if (!mIsShowing) return;

        mIsShowing = false;
        mUserEducationHelper.dismissTextBubble();
    }

    private boolean isLandscapePhone() {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mStatusView.getContext())
                && mStatusView.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;
    }

    boolean isShowingForTesting() {
        return mIsShowing;
    }
}
