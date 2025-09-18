// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.SnoozeAction;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Class that manages requests to trigger IPH's. Customizes the IPH with text bubbles, view
 * highlights, etc. based on the configuration. Recipes for use: 1. Create an IPH bubble anchored to
 * a view: mUserEducationHelper.requestShowIph(new IphCommandBuilder(myContext.getResources(),
 * FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 * R.string.my_feature_iph_accessibility_text) .setAnchorView(myAnchorView)
 * .setCircleHighlight(true) .build()); 2. Create an IPH bubble that does custom logic when shown
 * and hidden mUserEducationHelper.requestShowIph(new IphCommandBuilder(myContext.getResources(),
 * FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 * R.string.my_feature_iph_accessibility_text) .setAnchorView(myAnchorView)
 * .setCircleHighlight(true) .setOnShowCallback( ()-> doCustomShowLogic()) .setOnDismissCallback(()
 * -> doCustomDismissLogic()) .build());
 */
@NullMarked
public class UserEducationHelper {
    private final Activity mActivity;
    private final Handler mHandler;

    private Profile mProfile;
    private @Nullable List<IphCommand> mPendingIphCommands;
    private @Nullable TextBubble mTextBubble;

    /**
     * Constructs a {@link UserEducationHelper} that is immediately available to process inbound
     * {@link IphCommand}s.
     */
    public UserEducationHelper(Activity activity, Profile profile, Handler handler) {
        assert activity != null : "Trying to show an IPH for a null activity.";
        assert profile != null : "Trying to show an IPH with a null profile";

        mActivity = activity;
        mHandler = handler;

        setProfile(profile);
    }

    /**
     * Constructs a {@link UserEducationHelper} that will wait for a {@link Profile} to become
     * available before processing inbound {@link IphCommand}s.
     *
     * <p>Caveat, this will only observe the first available Profile from the supplier and will keep
     * a reference to the {@link Profile#getOriginalProfile()}.
     */
    public UserEducationHelper(
            Activity activity, Supplier<@Nullable Profile> profileSupplier, Handler handler) {
        mActivity = activity;
        mHandler = handler;

        SupplierUtils.waitForAll(
                () -> setProfile(assertNonNull(profileSupplier.get())), profileSupplier);
    }

    @Initializer
    private void setProfile(Profile profile) {
        assert profile != null;
        mProfile = profile.getOriginalProfile();

        if (mPendingIphCommands != null) {
            for (IphCommand iphCommand : mPendingIphCommands) {
                requestShowIph(iphCommand);
            }
            mPendingIphCommands = null;
        }
    }

    /**
     * Requests display of the in-product help (IPH) data in @param iphCommand.
     *
     * @see IphCommand for a breakdown of this data. Display will only occur if the feature
     *     engagement tracker for the current profile says it should.
     */
    public void requestShowIph(IphCommand iphCommand) {
        if (iphCommand == null) return;

        if (mProfile == null) {
            if (mPendingIphCommands == null) mPendingIphCommands = new ArrayList<>();
            mPendingIphCommands.add(iphCommand);
            return;
        }

        try (TraceEvent te = TraceEvent.scoped("UserEducationHelper::requestShowIph")) {
            final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
            tracker.addOnInitializedCallback(success -> showIph(tracker, iphCommand));
        }
    }

    private void showIph(Tracker tracker, IphCommand iphCommand) {
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
                        tracker.shouldTriggerHelpUi(featureName), /* shouldShowSnooze= */ false);

        assert (triggerDetails != null);
        if (!triggerDetails.shouldTriggerIph) {
            iphCommand.onBlockedCallback.run();
            return;
        }

        // iphCommand would have been built lazily, and we would have to fetch the data that is
        // needed from this point on.
        iphCommand.fetchFromResources();

        if (iphCommand.showTextBubble) {
            String contentString = assumeNonNull(iphCommand.contentString);
            String accessibilityString = assumeNonNull(iphCommand.accessibilityText);
            assert !contentString.isEmpty();
            assert !accessibilityString.isEmpty();

            mTextBubble =
                    new TextBubble(
                            mActivity,
                            anchorView,
                            contentString,
                            accessibilityString,
                            !iphCommand.removeArrow,
                            viewRectProvider != null
                                    ? viewRectProvider
                                    : assumeNonNull(rectProvider),
                            ChromeAccessibilityUtil.get().isAccessibilityEnabled());
            mTextBubble.setPreferredVerticalOrientation(iphCommand.preferredVerticalOrientation);
            mTextBubble.setDismissOnTouchInteraction(iphCommand.dismissOnTouch);
            mTextBubble.addOnDismissListener(
                    () -> {
                        mHandler.postDelayed(
                                () -> {
                                    if (featureName != null) {
                                        if (iphCommand.enableSnoozeMode) {
                                            final int snoozeAction =
                                                    mTextBubble != null
                                                                    && mTextBubble
                                                                            .wasDismissedByInsideTouch()
                                                            ? SnoozeAction.DISMISSED
                                                            : SnoozeAction.SNOOZED;
                                            tracker.dismissedWithSnooze(featureName, snoozeAction);
                                        } else {
                                            tracker.dismissed(featureName);
                                        }
                                    }
                                    iphCommand.onDismissCallback.run();
                                    if (highlightParams != null) {
                                        ViewHighlighter.turnOffHighlight(anchorView);
                                    }
                                    mTextBubble = null;
                                },
                                ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS);
                    });
            mTextBubble.setAutoDismissTimeout(iphCommand.autoDismissTimeout);
            if (iphCommand.dismissOnTouchTimeout != TextBubble.NO_TIMEOUT) {
                TextBubble textBubbleForLambda = mTextBubble;
                mHandler.postDelayed(
                        () -> textBubbleForLambda.setDismissOnTouchInteraction(true),
                        iphCommand.dismissOnTouchTimeout);
            }

            mTextBubble.show();
        }

        if (highlightParams != null) {
            ViewHighlighter.turnOnHighlight(anchorView, highlightParams);
        }

        if (viewRectProvider != null) {
            viewRectProvider.setInsetPx(assumeNonNull(iphCommand.insetRect));
        }

        iphCommand.onShowCallback.run();
    }

    /** Dismisses the currently showing text bubble, if any. */
    public void dismissTextBubble() {
        if (mTextBubble != null) {
            mTextBubble.dismiss();
        }
    }

    public @Nullable TextBubble getTextBubbleForTesting() {
        return mTextBubble;
    }
}
