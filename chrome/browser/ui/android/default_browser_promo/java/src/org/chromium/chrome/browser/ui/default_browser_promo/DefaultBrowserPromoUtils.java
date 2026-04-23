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
import org.chromium.components.search_engines.SearchEngineChoiceService;
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

    /** An interface to allow external components to suppress the default browser promo. */
    public interface DefaultBrowserPromoDelegate {
        /** Returns true if the default browser promo should be suppressed. */
        boolean shouldSuppressPromo();
    }

    private final DefaultBrowserPromoImpressionCounter mImpressionCounter;
    private final DefaultBrowserStateProvider mStateProvider;

    private static @Nullable DefaultBrowserPromoUtils sInstance;
    private static @Nullable DefaultBrowserPromoDelegate sDelegate;

    private final ObserverList<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListeners;

    @IntDef({
        DefaultBrowserPromoEntryPoint.APP_MENU,
        DefaultBrowserPromoEntryPoint.SETTINGS,
        DefaultBrowserPromoEntryPoint.SET_UP_LIST,
        DefaultBrowserPromoEntryPoint.CHROME_STARTUP,
        DefaultBrowserPromoEntryPoint.FRE,
        DefaultBrowserPromoEntryPoint.APP_MENU_RMD,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserPromoEntryPoint {
        int APP_MENU = 0;
        int SETTINGS = 1;
        int SET_UP_LIST = 2;
        int CHROME_STARTUP = 3;
        int FRE = 4;
        int APP_MENU_RMD = 5;
        int APP_MENU_DEEP_LINK = 6;
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

    public static void setDelegate(DefaultBrowserPromoDelegate delegate) {
        sDelegate = delegate;
    }

    public static @Nullable DefaultBrowserPromoDelegate getDelegateForTesting() {
        return sDelegate;
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
        if (source == DefaultBrowserPromoEntryPoint.FRE) {
            if (!shouldShowRoleManagerPromoForFre(activity)) return false;
        } else if (!shouldShowRoleManagerPromo(activity, source)) {
            return false;
        }

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
     * Determine if default browser promo other than the Role Manager Promo should be displayed.
     * This is typically used for the Message banner promo triggered by pasting URLs.
     *
     * <p>Conditions for showing: 1. The promo is not suppressed by the {@link
     * DefaultBrowserPromoDelegate} (e.g., the Setup List is active). 2. The Role Manager Promo
     * shouldn't be shown for the Startup entry point. 3. The impression count condition (ignoring
     * max count) is met. 4. The current default browser state satisfies the pre-defined conditions.
     */
    public boolean shouldShowNonRoleManagerPromo(Context context) {
        if (sDelegate != null && sDelegate.shouldSuppressPromo()) {
            return false;
        }

        return !shouldShowRoleManagerPromo(context, DefaultBrowserPromoEntryPoint.CHROME_STARTUP)
                && mImpressionCounter.shouldShowPromo(/* ignoreMaxCount= */ true)
                && mStateProvider.shouldShowPromo();
    }

    /**
     * This decides whether the dialog should be promoted. Returns true if: the feature is enabled,
     * the {@link RoleManager} is available, the promo is not suppressed by the {@link
     * DefaultBrowserPromoDelegate}, and both the impression count and current default browser state
     * satisfy the pre-defined conditions.
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

        // Suppress the passive startup promo if the delegate (e.g. Setup List) says so.
        if (source == DefaultBrowserPromoEntryPoint.CHROME_STARTUP
                && sDelegate != null
                && sDelegate.shouldSuppressPromo()) {
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

    /**
     * This is a specialized version of {@link #shouldShowRoleManagerPromo(Context)} that determines
     * whether the Role Manager Promo should be shown during the First Run Experience.
     */
    public boolean shouldShowRoleManagerPromoForFre(Context context) {
        if (!isFeatureEnabled()) return false;

        // For FRE, roleManager.isRoleHeld(RoleManager.ROLE_BROWSER) actually just returns false
        // even if Chrome (Canary, Dev, Beta, Stable) is set as default. But we're still calling
        // this method because it checks whether the role is available.
        if (!isRoleAvailableButNotHeld(context)) return false;

        // getSessionCount will always be 0 for FRE, and MIN_TRIGGER_SESSION_COUNT is 3. We
        // therefore skip checking session counts for FRE promo.
        boolean isCountAndIntervalOk =
                (mImpressionCounter.getPromoCount() < mImpressionCounter.getMaxPromoCount())
                        && (mImpressionCounter.getLastPromoInterval()
                                >= mImpressionCounter.getMinPromoInterval())
                        && !SearchEngineChoiceService.getInstance()
                                .isDefaultBrowserPromoSuppressed();

        if (!isCountAndIntervalOk) return false;

        // Only show promo on FRE if Chrome (Canary, Dev, Beta, Stable) is not set as default.
        int state = mStateProvider.getCurrentDefaultBrowserState();
        if (state == DefaultBrowserState.CHROME_DEFAULT
                || state == DefaultBrowserState.OTHER_CHROME_DEFAULT) {
            return false;
        }
        return true;
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

                        // Record how many people saw the RMD through the app menu item.
                        if (source == DefaultBrowserPromoEntryPoint.APP_MENU) {
                            DefaultBrowserPromoMetrics.recordEntrypointClick(
                                    DefaultBrowserPromoEntryPoint.APP_MENU_RMD, currentState);
                        }

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

                        // Save the source to SharedPreferences so we can check it upon returning to
                        // Chrome.
                        ChromeSharedPreferences.getInstance()
                                .writeInt(
                                        ChromePreferenceKeys
                                                .DEFAULT_BROWSER_PROMO_DEEP_LINK_COMPARE_OUTCOME_SOURCE,
                                        source);

                        // Record the specific deep-link source.
                        if (source == DefaultBrowserPromoEntryPoint.APP_MENU) {
                            DefaultBrowserPromoMetrics.recordPromoClick(
                                    DefaultBrowserPromoMetrics.DefaultBrowserPromoSourceType
                                            .APP_MENU_DEEPLINK);
                        } else if (source == DefaultBrowserPromoEntryPoint.SETTINGS) {
                            DefaultBrowserPromoMetrics.recordPromoClick(
                                    DefaultBrowserPromoMetrics.DefaultBrowserPromoSourceType
                                            .SETTINGS_ROW_DEEPLINK);
                        }

                        openSystemDefaultAppsSettings(activity);
                    }
                });
    }

    /**
     * Checks if there is an outcome to record from a previous deep-link. Called from
     * ChromeTabbedActivity#onResumeWithNative.
     */
    public void maybeRecordDeepLinkOutcome() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        // 0 for AppMenu, 1 for Settings.
        int beforeSource =
                prefs.readInt(
                        ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_DEEP_LINK_COMPARE_OUTCOME_SOURCE,
                        -1);

        if (beforeSource == -1) return;

        // Clear the SharedPreference immediately.
        prefs.removeKey(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_DEEP_LINK_COMPARE_OUTCOME_SOURCE);

        int outcomeSource;
        if (beforeSource == DefaultBrowserPromoEntryPoint.APP_MENU) {
            // Promo in App Menu can either show the RMD or deep-link. This method only records
            // deep-links.
            outcomeSource = DefaultBrowserPromoEntryPoint.APP_MENU_DEEP_LINK;
        } else {
            // Promo in Main Settings always deep links.
            outcomeSource = beforeSource;
        }

        // We need to fetch the default browser info again to make sure the state is
        // updated before recording the outcome.
        fetchDefaultBrowserInfo(
                info -> {
                    if (info != null) {
                        DefaultBrowserPromoMetrics.recordOutcome(
                                info.defaultBrowserState, outcomeSource);
                    }
                });
    }

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
