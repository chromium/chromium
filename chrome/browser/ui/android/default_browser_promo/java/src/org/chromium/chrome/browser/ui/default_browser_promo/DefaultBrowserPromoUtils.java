// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.os.Build;

import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.ui.base.WindowAndroid;

/** A utility class providing information regarding states of default browser. */
@NullMarked
public class DefaultBrowserPromoUtils {
    /**
     * An interface for receiving updates related to the trigger state of the default browser promo.
     */
    public interface DefaultBrowserPromoTriggerStateListener {
        /** Called when a default browser promo becomes visible to the user. */
        void onDefaultBrowserPromoTriggered();
    }

    private final DefaultBrowserPromoImpressionCounter mImpressionCounter;
    private final DefaultBrowserStateProvider mStateProvider;

    private static @Nullable DefaultBrowserPromoUtils sInstance;

    private final ObserverList<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListeners;

    DefaultBrowserPromoUtils(
            DefaultBrowserPromoImpressionCounter impressionCounter,
            DefaultBrowserStateProvider stateProvider) {
        mImpressionCounter = impressionCounter;
        mStateProvider = stateProvider;
        mDefaultBrowserPromoTriggerStateListeners = new ObserverList<>();
    }

    public static DefaultBrowserPromoUtils getInstance() {
        if (sInstance == null) {
            sInstance =
                    new DefaultBrowserPromoUtils(
                            new DefaultBrowserPromoImpressionCounter(),
                            new DefaultBrowserStateProvider());
        }
        return sInstance;
    }

    static boolean isFeatureEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO);
    }

    /**
     * Determine whether a promo dialog should be displayed or not. And prepare related logic to
     * launch promo if a promo dialog has been decided to display.
     *
     * @param activity The context.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @param track A {@link Tracker} for tracking role manager promo shown event.
     * @param ignoreMaxCount Whether the promo dialog should be shown irrespective of whether it has
     *     been shown before
     * @return True if promo dialog will be displayed.
     */
    public boolean prepareLaunchPromoIfNeeded(
            Activity activity, WindowAndroid windowAndroid, Tracker tracker) {
        if (!shouldShowRoleManagerPromo(activity)) return false;
        mImpressionCounter.onPromoShown();
        tracker.notifyEvent("role_manager_default_browser_promos_shown");
        DefaultBrowserPromoManager manager =
                new DefaultBrowserPromoManager(
                        activity, windowAndroid, mImpressionCounter, mStateProvider);
        manager.promoByRoleManager();
        return true;
    }

    /**
     * Show the default browser promo message if conditions are met.
     *
     * @param context The context.
     * @param windowAndroid The {@link WindowAndroid} for message to dispatch.
     * @param profile A {@link Profile} for checking incognito and getting the {@link Tracker} to
     *     tack promo impressions.
     */
    public void maybeShowDefaultBrowserPromoMessages(
            Context context, WindowAndroid windowAndroid, Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)) {
            return;
        }

        if (profile == null || profile.isOffTheRecord()) {
            return;
        }

        MessageDispatcher dispatcher = MessageDispatcherProvider.from(windowAndroid);
        if (dispatcher == null) {
            return;
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (shouldShowNonRoleManagerPromo(context)
                && tracker.shouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MESSAGES)) {
            notifyDefaultBrowserPromoVisible();
            DefaultBrowserPromoMessageController messageController =
                    new DefaultBrowserPromoMessageController(context, tracker);
            messageController.promo(dispatcher);
        }
    }

    /**
     * Determine if default browser promo other than the Role Manager Promo should be displayed:
     * 1. Role Manager Promo shouldn't be shown,
     * 2. Impression count condition, other than the max count for RoleManager is met,
     * 3. Current default browser state satisfied the pre-defined conditions.
     */
    public boolean shouldShowNonRoleManagerPromo(Context context) {
        return !shouldShowRoleManagerPromo(context)
                && mImpressionCounter.shouldShowPromo(/* ignoreMaxCount= */ true)
                && mStateProvider.shouldShowPromo();
    }

    /**
     * This decides whether the dialog should be promoted. Returns true if: the feature is enabled,
     * the {@link RoleManager} is available, and both the impression count and current default
     * browser state satisfied the pre-defined conditions.
     */
    public boolean shouldShowRoleManagerPromo(Context context) {
        if (!isFeatureEnabled()) return false;

        if (!isRoleAvailableButNotHeld(context)) {
            // Returns false if RoleManager default app setting is not available in the current
            // system, or the browser role is already held.
            return false;
        }

        return mImpressionCounter.shouldShowPromo(/* ignoreMaxCount= */ false)
                && mStateProvider.shouldShowPromo();
    }

    /** Increment session count for triggering feature in the future. */
    public static void incrementSessionCount() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT);
    }

    /**
     * Registers a {@link DefaultBrowserPromoTriggerStateListener} to receive updates when a default
     * browser promo becomes visible to the user.
     */
    public void addListener(DefaultBrowserPromoTriggerStateListener listener) {
        mDefaultBrowserPromoTriggerStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the list of state listeners.
     *
     * @param listener The listener to remove.
     */
    public void removeListener(DefaultBrowserPromoTriggerStateListener listener) {
        mDefaultBrowserPromoTriggerStateListeners.removeObserver(listener);
    }

    /** Notifies listeners that a default browser promo is now visible to the user. */
    public void notifyDefaultBrowserPromoVisible() {
        for (DefaultBrowserPromoTriggerStateListener listener :
                mDefaultBrowserPromoTriggerStateListeners) {
            listener.onDefaultBrowserPromoTriggered();
        }
    }

    @SuppressLint("NewApi")
    private boolean isRoleAvailableButNotHeld(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return false;
        }
        RoleManager roleManager = (RoleManager) context.getSystemService(Context.ROLE_SERVICE);
        if (roleManager == null) return false;
        boolean isRoleAvailable = roleManager.isRoleAvailable(RoleManager.ROLE_BROWSER);
        boolean isRoleHeld = roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
        return isRoleAvailable && !isRoleHeld;
    }

    public static void setInstanceForTesting(DefaultBrowserPromoUtils testInstance) {
        var oldInstance = sInstance;
        sInstance = testInstance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }
}
