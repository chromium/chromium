// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * Class that manages requests to trigger IPH's. Customizes the IPH with text bubbles, view
 * highlights, etc. based on the configuration.
 * Recipes for use:
 * 1. Create an IPH bubble anchored to a view:
 * mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(myContext.getResources(),
 *                 FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 *                 R.string.my_feature_iph_accessibility_text)
 *                                                     .setAnchorView(myAnchorView)
 *                                                     .setCircleHighlight(true)
 *                                                     .build());
 * 2. Create an IPH bubble that does custom logic when shown and hidden
 * mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(myContext.getResources(),
 *                 FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 *                 R.string.my_feature_iph_accessibility_text)
 *                                                     .setAnchorView(myAnchorView)
 *                                                     .setCircleHighlight(true)
 *                                                     .setOnShowCallback( ()-> doCustomShowLogic())
 *                                                     .setOnDismissCallback(() ->
 *                                                         doCustomDismissLogic())
 *                                                     .build());
 */
public class UserEducationHelper {
    private final Activity mActivity;
    private final Handler mHandler;

    private Profile mProfile;
    private List<IPHCommand> mPendingIPHCommands;
    private TextBubble mTextBubble;

    /**
     * Constructs a {@link UserEducationHelper} that is immediately available to process inbound
     * {@link IPHCommand}s.
     */
    public UserEducationHelper(
            @NonNull Activity activity, @NonNull Profile profile, Handler handler) {
        assert activity != null : "Trying to show an IPH for a null activity.";
        assert profile != null : "Trying to show an IPH with a null profile";

        mActivity = activity;
        mHandler = handler;

        setProfile(profile);
    }

    /**
     * Constructs a {@link UserEducationHelper} that will wait for a {@link Profile} to become
     * available before processing inbound {@link IPHCommand}s.
     *
     * <p>Caveat, this will only observe the first available Profile from the supplier and will keep
     * a reference to the {@link Profile#getOriginalProfile()}.
     */
    public UserEducationHelper(
            @NonNull Activity activity,
            @NonNull Supplier<Profile> profileSupplier,
            Handler handler) {
        assert activity != null : "Trying to show an IPH for a null activity.";
        assert profileSupplier != null : "Trying to show an IPH with a null profile supplier";

        mActivity = activity;
        mHandler = handler;

        SupplierUtils.waitForAll(() -> setProfile(profileSupplier.get()), profileSupplier);
    }

    private void setProfile(Profile profile) {
        assert profile != null;
        mProfile = profile.getOriginalProfile();

        if (mPendingIPHCommands != null) {
            for (IPHCommand iphCommand : mPendingIPHCommands) {
                requestShowIPH(iphCommand);
            }
            mPendingIPHCommands = null;
        }
    }

    /**
     * Requests display of the in-product help (IPH) data in @param iphCommand.
     * @see IPHCommand for a breakdown of this data.
     * Display will only occur if the feature engagement tracker for the current profile says it
     * should.
     */
    public void requestShowIPH(IPHCommand iphCommand) {
        if (iphCommand == null) return;

        if (mProfile == null) {
            if (mPendingIPHCommands == null) mPendingIPHCommands = new ArrayList<>();
            mPendingIPHCommands.add(iphCommand);
            return;
        }

        try (TraceEvent te = TraceEvent.scoped("UserEducationHelper::requestShowIPH")) {
            final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
            tracker.addOnInitializedCallback(success -> showIPH(tracker, iphCommand));
        }
    }

    private void showIPH(Tracker tracker, IPHCommand iphCommand) {
        // Activity was destroyed; don't show IPH.
        View anchorView = iphCommand.anchorView;
        if (mActivity == null
                || mActivity.isFinishing()
                || mActivity.isDestroyed()
                || anchorView == null) {
            iphCommand.onBlockedCallback.run();
            return;
        }

        String featureName = iphCommand.featureName;
        assert (featureName != null);

        ViewRectProvider viewRectProvider = iphCommand.viewRectProvider;
        RectProvider rectProvider =
                iphCommand.anchorRect != null ? new RectProvider(iphCommand.anchorRect) : null;
        if (viewRectProvider == null && rectProvider == null) {
            viewRectProvider = new ViewRectProvider(anchorView);
        }

        HighlightParams highlightParams = iphCommand.highlightParams;
        mTextBubble = null;
        TriggerDetails triggerDetails =
                new TriggerDetails(
                        tracker.shouldTriggerHelpUI(featureName), /* shouldShowSnooze= */ false);

        assert (triggerDetails != null);
        if (!triggerDetails.shouldTriggerIph) {
            iphCommand.onBlockedCallback.run();
            return;
        }

        // iphCommand would have been built lazily, and we would have to fetch the data that is
        // needed from this point on.
        iphCommand.fetchFromResources();

        if (iphCommand.showTextBubble) {
            String contentString = iphCommand.contentString;
            String accessibilityString = iphCommand.accessibilityText;
            assert !contentString.isEmpty();
            assert !accessibilityString.isEmpty();

            mTextBubble =
                    new TextBubble(
                            mActivity,
                            anchorView,
                            contentString,
                            accessibilityString,
                            !iphCommand.removeArrow,
                            viewRectProvider != null ? viewRectProvider : rectProvider,
                            ChromeAccessibilityUtil.get().isAccessibilityEnabled());
            mTextBubble.setPreferredVerticalOrientation(iphCommand.preferredVerticalOrientation);
            mTextBubble.setDismissOnTouchInteraction(iphCommand.dismissOnTouch);
            mTextBubble.addOnDismissListener(
                    () ->
                            mHandler.postDelayed(
                                    () -> {
                                        if (featureName != null) tracker.dismissed(featureName);
                                        iphCommand.onDismissCallback.run();
                                        if (highlightParams != null) {
                                            ViewHighlighter.turnOffHighlight(anchorView);
                                        }
                                        mTextBubble = null;
                                    },
                                    ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));
            mTextBubble.setAutoDismissTimeout(iphCommand.autoDismissTimeout);

            mTextBubble.show();
        }

        if (highlightParams != null) {
            ViewHighlighter.turnOnHighlight(anchorView, highlightParams);
        }

        if (viewRectProvider != null) {
            viewRectProvider.setInsetPx(iphCommand.insetRect);
        }

        iphCommand.onShowCallback.run();
    }

    /** Dismisses the currently showing text bubble, if any. */
    public void dismissTextBubble() {
        if (mTextBubble != null) {
            mTextBubble.dismiss();
        }
    }
}
