// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingEvent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingType;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

@NullMarked
public class BottomBarEducationDialogCoordinator
        implements ModalDialogProperties.Controller, Destroyable, ComponentCallbacks {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable Tracker mTracker;
    private @Nullable View mDialogView;
    private @State int mState = State.IDLE;

    @IntDef({State.IDLE, State.SHOWING, State.OUTCOME_RECORDED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        /** The dialog hasn't been shown yet. */
        int IDLE = 0;

        /** The dialog is showing or queued to show. */
        int SHOWING = 1;

        /** Terminal state: the outcome was recorded, so the dialog won't show again. */
        int OUTCOME_RECORDED = 2;
    }

    /**
     * Constructor for the BottomBarEducationDialogCoordinator.
     *
     * @param context The current context.
     * @param modalDialogManager The ModalDialogManager to show the dialog.
     * @param tabSupplier Supplier of the current tab.
     */
    public BottomBarEducationDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            Supplier<@Nullable Tab> tabSupplier) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTabSupplier = tabSupplier;
        mContext.registerComponentCallbacks(this);
    }

    @Override
    public void destroy() {
        mContext.unregisterComponentCallbacks(this);
        if (mDialogModel != null) {
            maybeRecordOutcome(EducationOnboardingEvent.DISMISSED_OTHER);
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
            mDialogModel = null;
            mDialogView = null;
        }
    }

    /**
     * Attempts to show the opt-in education dialog if the user is eligible.
     *
     * @param profile The current Profile.
     * @return Whether the dialog was actually shown.
     */
    public boolean maybeShow(Profile profile) {
        if (mState != State.IDLE) return mState == State.SHOWING;

        if (!BottomBarConfigUtils.isEligibleForEducationDialog(mContext)) {
            return false;
        }

        Tab tab = mTabSupplier.get();
        if (profile.isOffTheRecord() || tab == null || tab.isOffTheRecord()) {
            return false;
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile.getOriginalProfile());
        if (!tracker.isInitialized()
                || !tracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION)) {
            return false;
        }

        mTracker = tracker;

        showDialog();
        // Setting the flag at request time is acceptable even if the dialog queues because it only
        // defers the NTP tip.
        BottomBarEducationCoordinator.setEducationShownThisSession();
        return true;
    }

    private void showDialog() {
        mState = State.SHOWING;
        mDialogView =
                BottomBarEducationUtils.inflateDialogContent(
                        mContext, /* onSettingsClicked= */ this::onLearnMoreClicked);

        Resources resources = mContext.getResources();
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.bottom_bar_education_try_it_now)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.bottom_bar_education_not_now)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .build();

        mModalDialogManager.showDialog(
                mDialogModel, ModalDialogManager.ModalDialogType.APP, /* showAsNext= */ true);
        BottomBarMetrics.recordEducationOnboardingEvent(
                EducationOnboardingType.OPT_IN_DIALOG, EducationOnboardingEvent.SHOWN);
    }

    private void onPositiveClicked() {
        if (!maybeRecordOutcome(EducationOnboardingEvent.ACCEPTED)) return;

        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
        BottomBarConfigUtils.setBottomBarUserEnabled(/* enabled= */ true);
    }

    private void onNegativeClicked() {
        if (!maybeRecordOutcome(EducationOnboardingEvent.DECLINED)) return;

        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    private void onLearnMoreClicked() {
        if (!maybeRecordOutcome(EducationOnboardingEvent.SETTINGS_CLICKED)) return;

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsNavigation.SettingsFragment.APPEARANCE);
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        }
    }

    /**
     * Records {@code event}, notifies the tracker and moves to {@link State#OUTCOME_RECORDED} if
     * the dialog is {@link State#SHOWING}. Returns whether this call recorded the outcome.
     */
    private boolean maybeRecordOutcome(@EducationOnboardingEvent int event) {
        if (mState != State.SHOWING) return false;
        mState = State.OUTCOME_RECORDED;

        BottomBarMetrics.recordEducationOnboardingEvent(
                EducationOnboardingType.OPT_IN_DIALOG, event);

        Tracker tracker = mTracker;
        if (tracker != null) {
            if (event == EducationOnboardingEvent.ACCEPTED) {
                tracker.notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_USED);
                tracker.notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_OPT_IN_ACCEPTED);
            }
            tracker.dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        }
        return true;
    }

    private void updateIllustrationVisibility(Configuration config) {
        if (mDialogView != null) {
            BottomBarEducationUtils.updateIllustrationVisibility(mDialogView, config);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        updateIllustrationVisibility(newConfig);
    }

    @Override
    public void onLowMemory() {}

    @Override
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            onPositiveClicked();
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            onNegativeClicked();
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        // CANCEL_ON_TOUCH_OUTSIDE is false, so NAVIGATE_BACK_OR_TOUCH_OUTSIDE can only
        // come from back.
        @EducationOnboardingEvent
        int event =
                dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                        ? EducationOnboardingEvent.BACK_PRESSED
                        : EducationOnboardingEvent.DISMISSED_OTHER;
        maybeRecordOutcome(event);
        mDialogModel = null;
        mDialogView = null;
        mTracker = null;
    }
}
