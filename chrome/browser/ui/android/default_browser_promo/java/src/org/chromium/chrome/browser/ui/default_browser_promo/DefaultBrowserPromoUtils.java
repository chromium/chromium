// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;

/** A utility class providing information regarding states of default browser. */
@NullMarked
public class DefaultBrowserPromoUtils {
    private static final long COOLDOWN_PERIOD_MS = Duration.ofDays(7).toMillis();

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

    @IntDef({
        DefaultBrowserPromoEntryPoint.APP_MENU,
        DefaultBrowserPromoEntryPoint.SETTINGS,
        DefaultBrowserPromoEntryPoint.SET_UP_LIST,
        DefaultBrowserPromoEntryPoint.CHROME_STARTUP
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserPromoEntryPoint {
        int APP_MENU = 0;
        int SETTINGS = 1;
        int SET_UP_LIST = 2;
        int CHROME_STARTUP = 3;
    }

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
     * @param tracker A {@link Tracker} for tracking role manager promo shown event.
     * @param source The source of the click, one of {@link DefaultBrowserPromoEntryPoint}.
     * @return True if promo dialog will be displayed.
     */
    public boolean prepareLaunchPromoIfNeeded(
            Activity activity,
            WindowAndroid windowAndroid,
            Tracker tracker,
            @DefaultBrowserPromoEntryPoint int source) {
        if (!shouldShowRoleManagerPromo(activity, source)) return false;
        mImpressionCounter.onPromoShown();
        tracker.notifyEvent("role_manager_default_browser_promos_shown");
        DefaultBrowserPromoManager manager =
                new DefaultBrowserPromoManager(
                        activity, windowAndroid, mImpressionCounter, mStateProvider, source);
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
     * Determine if default browser promo other than the Role Manager Promo should be displayed: 1.
     * Role Manager Promo shouldn't be shown, 2. Impression count condition, other than the max
     * count for RoleManager is met, 3. Current default browser state satisfied the pre-defined
     * conditions.
     */
    public boolean shouldShowNonRoleManagerPromo(Context context) {
        return !shouldShowRoleManagerPromo(context, DefaultBrowserPromoEntryPoint.CHROME_STARTUP)
                && mImpressionCounter.shouldShowPromo(/* ignoreMaxCount= */ true)
                && mStateProvider.shouldShowPromo();
    }

    /**
     * This decides whether the dialog should be promoted. Returns true if: the feature is enabled,
     * the {@link RoleManager} is available, and both the impression count and current default
     * browser state satisfied the pre-defined conditions.
     *
     * @param context The context.
     * @param source The source of the click, one of {@link DefaultBrowserPromoEntryPoint}.
     */
    public boolean shouldShowRoleManagerPromo(
            Context context, @DefaultBrowserPromoEntryPoint int source) {
        if (!isFeatureEnabled()) return false;

        if (!isRoleAvailableButNotHeld(context)) {
            // Returns false if RoleManager default app setting is not available in the current
            // system, or the browser role is already held.
            return false;
        }

        int promoCount = mImpressionCounter.getPromoCount();
        if (source != DefaultBrowserPromoEntryPoint.CHROME_STARTUP) {
            // For explicit actions (App menu, Settings, Setup List), we only check if the
            // Role Manager has been shown before.
            return promoCount == 0;
        }

        // For passive promos (Startup), we also check session counts and browser state conditions.
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
        setLastDefaultBrowserPromoTimestamp();
        for (DefaultBrowserPromoTriggerStateListener listener :
                mDefaultBrowserPromoTriggerStateListeners) {
            listener.onDefaultBrowserPromoTriggered();
        }
    }

    /** Records the current time as the last-shown timestamp for any Default Browser Promo. */
    private void setLastDefaultBrowserPromoTimestamp() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeLong(
                EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP,
                TimeUtils.currentTimeMillis());
    }

    /**
     * Checks if any Default Browser Promo has been shown within the defined cooldown period
     * (currently 7 days). This function reads the last-shown timestamp from SharedPreferences and
     * compares it against the current time.
     */
    public static boolean hasPromoShownRecently() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        long lastShownTime =
                prefsManager.readLong(EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP, 0);

        if (lastShownTime == 0) {
            return false; // Never shown
        }

        return (TimeUtils.currentTimeMillis() - lastShownTime) < COOLDOWN_PERIOD_MS;
    }

    @SuppressLint("NewApi")
    private boolean isRoleAvailableButNotHeld(Context context) {
        RoleManager roleManager = (RoleManager) context.getSystemService(Context.ROLE_SERVICE);
        if (roleManager == null) return false;
        boolean isRoleAvailable = roleManager.isRoleAvailable(RoleManager.ROLE_BROWSER);
        boolean isRoleHeld = roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
        return isRoleAvailable && !isRoleHeld;
    }

    /**
     * @return True if: 1. sDefaultBrowserPromoEntryPoint is enabled. 2. Chrome is not the default
     *     browser. Used by surfaces such as the app menu, in which the menu item doesn't persist.
     */
    public boolean shouldShowAppMenuItemEntryPoint() {
        return ChromeFeatureList.sDefaultBrowserPromoEntryPoint.isEnabled()
                && mStateProvider.shouldShowPromo();
    }

    private void openSystemDefaultAppsSettings(Context context) {
        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.safeStartActivity(context, intent);
    }

    /**
     * Shared logic for handling the click on the menu items (default promo entry points) in
     * Settings & App Menu. Note that the Role Manager dialog is shown if eligible; otherwise falls
     * back to the Android System Settings default apps page.
     *
     * @param activity The current activity.
     * @param windowAndroid The WindowAndroid (required for Role Manager).
     * @param source The source of the click, one of {@link DefaultBrowserPromoEntryPoint}.
     */
    public void onMenuItemClick(
            Activity activity,
            @Nullable WindowAndroid windowAndroid,
            @DefaultBrowserPromoEntryPoint int source) {

        fetchDefaultBrowserInfo(
                info -> {
                    if (info == null) {
                        // Fallback: show the default apps page in Android settings.
                        openSystemDefaultAppsSettings(activity);
                        return;
                    }

                    // Record the volume/engagement.
                    @DefaultBrowserState int currentState = info.defaultBrowserState;
                    DefaultBrowserPromoMetrics.recordEntrypointClick(source, currentState);

                    if (windowAndroid != null && shouldShowRoleManagerPromo(activity, source)) {
                        mImpressionCounter.onPromoShown();
                        DefaultBrowserPromoManager manager =
                                new DefaultBrowserPromoManager(
                                        activity,
                                        windowAndroid,
                                        mImpressionCounter,
                                        mStateProvider,
                                        source);
                        manager.promoByRoleManager();
                    } else {
                        // Fallback: show the default apps page in Android settings.
                        openSystemDefaultAppsSettings(activity);
                    }
                });
    }

    @VisibleForTesting
    protected void fetchDefaultBrowserInfo(
            Callback<DefaultBrowserInfo.@Nullable DefaultInfo> callback) {
        // Force clear the old cached value and generate a fresh DefaultInfoTask.
        DefaultBrowserInfo.resetDefaultInfoTask();
        // Fetch the fresh info asynchronously.
        DefaultBrowserInfo.getDefaultBrowserInfo(callback);
    }

    public static void setInstanceForTesting(DefaultBrowserPromoUtils testInstance) {
        var oldInstance = sInstance;
        sInstance = testInstance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }
}
