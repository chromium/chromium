// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface for navigating Settings. */
@NullMarked
public interface SettingsNavigation {
    @IntDef({
        SettingsFragment.ABOUT_CHROME,
        SettingsFragment.ACCESSIBILITY,
        SettingsFragment.ADAPTIVE_TOOLBAR,
        SettingsFragment.ADDRESS_BAR,
        SettingsFragment.ALL_SITES,
        SettingsFragment.ANDROID_PAYMENT_APPS,
        SettingsFragment.APPEARANCE,
        SettingsFragment.AUTOFILL_AND_PASSWORDS,
        SettingsFragment.AUTOFILL_BUY_NOW_PAY_LATER,
        SettingsFragment.AUTOFILL_CARD_BENEFITS,
        SettingsFragment.AUTOFILL_IDENTITY_DOCS,
        SettingsFragment.AUTOFILL_OPTIONS,
        SettingsFragment.AUTOFILL_PERSONAL_CONTEXT,
        SettingsFragment.AUTOFILL_PROFILES,
        SettingsFragment.AUTOFILL_SHOPPING,
        SettingsFragment.AUTOFILL_TRAVEL,
        SettingsFragment.CHOSEN_OBJECT,
        SettingsFragment.CLEAR_BROWSING_DATA,
        SettingsFragment.CONTEXTUAL_SEARCH,
        SettingsFragment.COOKIES,
        SettingsFragment.DEVELOPER,
        SettingsFragment.DO_NOT_TRACK,
        SettingsFragment.DOWNLOADS,
        SettingsFragment.FINANCIAL_ACCOUNTS,
        SettingsFragment.GLIC,
        SettingsFragment.GLIC_PERMISSIONS,
        SettingsFragment.GOOGLE_SERVICES,
        SettingsFragment.GROUPED_WEBSITES,
        SettingsFragment.HOMEPAGE,
        SettingsFragment.HTTPS_FIRST_MODE,
        SettingsFragment.IMAGE_DESCRIPTIONS,
        SettingsFragment.LANGUAGE,
        SettingsFragment.LEGAL_INFORMATION,
        SettingsFragment.LOCATION_PERMISSION,
        SettingsFragment.MAIN, // The main settings page.
        SettingsFragment.MANAGE_SYNC,
        SettingsFragment.NON_CARD_PAYMENT_METHODS,
        SettingsFragment.PAGE_INFO_AD_PERSONALIZATION,
        SettingsFragment.PAGE_INFO_COOKIES,
        SettingsFragment.PAYMENT_METHODS,
        SettingsFragment.PERSONALIZE_GOOGLE_SERVICES,
        SettingsFragment.PRELOAD_PAGES,
        SettingsFragment.PRELOAD_PAGES_EXTENDED,
        SettingsFragment.PRELOAD_PAGES_STANDARD,
        SettingsFragment.PRICE_NOTIFICATION,
        SettingsFragment.PRIVACY,
        SettingsFragment.SAFE_BROWSING,
        SettingsFragment.SAFE_BROWSING_ENHANCED,
        SettingsFragment.SAFE_BROWSING_STANDARD,
        SettingsFragment.SAFETY_CHECK,
        SettingsFragment.SAFETY_CHECK_SETTINGS,
        SettingsFragment.SEARCH_ENGINE,
        SettingsFragment.SEARCH_RESULTS,
        SettingsFragment.SECURE_DNS,
        SettingsFragment.SINGLE_CATEGORY,
        SettingsFragment.SINGLE_WEBSITE,
        SettingsFragment.SITE,
        SettingsFragment.SITE_SEARCH,
        SettingsFragment.STORAGE_ACCESS,
        SettingsFragment.TAB_ARCHIVE,
        SettingsFragment.TABS,
        SettingsFragment.THEME,
        SettingsFragment.TRACING,
        SettingsFragment.TRACING_CATEGORIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SettingsFragment {
        int ABOUT_CHROME = 0;
        int ACCESSIBILITY = 1;
        // int AD_MEASUREMENT = 2; (OBSOLETE)
        int ADAPTIVE_TOOLBAR = 3;
        int ADDRESS_BAR = 4;
        int ALL_SITES = 5;
        int ANDROID_PAYMENT_APPS = 6;
        int APPEARANCE = 7;
        int AUTOFILL_AND_PASSWORDS = 28;
        int AUTOFILL_BUY_NOW_PAY_LATER = 8;
        int AUTOFILL_CARD_BENEFITS = 9;
        int AUTOFILL_IDENTITY_DOCS = 10;
        int AUTOFILL_OPTIONS = 11;
        int AUTOFILL_PERSONAL_CONTEXT = 12;
        int AUTOFILL_PROFILES = 13;
        int AUTOFILL_SHOPPING = 14;
        int AUTOFILL_TRAVEL = 15;
        int CHOSEN_OBJECT = 16;
        int CLEAR_BROWSING_DATA = 17;
        int CONTEXTUAL_SEARCH = 18;
        int COOKIES = 19;
        int DEVELOPER = 20;
        int DO_NOT_TRACK = 21;
        int DOWNLOADS = 22;
        int FINANCIAL_ACCOUNTS = 23;
        int GLIC = 24;
        int GLIC_PERMISSIONS = 25;
        int GOOGLE_SERVICES = 26;
        int GROUPED_WEBSITES = 27;
        int HOMEPAGE = 29;
        int HTTPS_FIRST_MODE = 30;
        int IMAGE_DESCRIPTIONS = 31;
        int LANGUAGE = 32;
        int LEGAL_INFORMATION = 33;
        int LOCATION_PERMISSION = 34;
        int MAIN = 35; // The main settings page.
        int MANAGE_SYNC = 36;
        int NON_CARD_PAYMENT_METHODS = 37;
        int PAGE_INFO_AD_PERSONALIZATION = 38;
        int PAGE_INFO_COOKIES = 39;
        int PAYMENT_METHODS = 40;
        int PERSONALIZE_GOOGLE_SERVICES = 41;
        int PRELOAD_PAGES = 42;
        int PRELOAD_PAGES_EXTENDED = 43;
        int PRELOAD_PAGES_STANDARD = 44;
        int PRICE_NOTIFICATION = 45;
        int PRIVACY = 46;
        // int PRIVACY_SANDBOX_FLEDGE = 47; (OBSOLETE)
        // int PRIVACY_SANDBOX_FLEDGE_ALL_SITES = 48; (OBSOLETE)
        // int PRIVACY_SANDBOX_FLEDGE_BLOCKED_SITES = 49; (OBSOLETE)
        // int PRIVACY_SANDBOX_FLEDGE_LEARN_MORE = 50; (OBSOLETE)
        // int PRIVACY_SANDBOX_SETTINGS = 51; (OBSOLETE)
        // int PRIVACY_SANDBOX_TOPICS = 52; (OBSOLETE)
        // int PRIVACY_SANDBOX_TOPICS_BLOCKED = 53; (OBSOLETE)
        // int PRIVACY_SANDBOX_TOPICS_MANAGE = 54; (OBSOLETE)
        int SAFE_BROWSING = 55;
        int SAFE_BROWSING_ENHANCED = 56;
        int SAFE_BROWSING_STANDARD = 57;
        int SAFETY_CHECK = 58;
        int SAFETY_CHECK_SETTINGS = 59;
        int SEARCH_ENGINE = 60;
        int SEARCH_RESULTS = 61;
        int SECURE_DNS = 62;
        int SINGLE_CATEGORY = 63;
        int SINGLE_WEBSITE = 64;
        int SITE = 65; // Site settings.
        int SITE_SEARCH = 66;
        int STORAGE_ACCESS = 67;
        int TAB_ARCHIVE = 68;
        int TABS = 69;
        int THEME = 70;
        int TRACING = 71;
        int TRACING_CATEGORIES = 72;
    }

    /**
     * Starts settings with the default (top-level) fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     */
    void startSettings(Context context);

    /**
     * Starts a specific settings fragment. This can be used by code that does not supply its own
     * settings page, but instead needs to redirect the user to an appropriate page that is out of
     * reach.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param settingsFragment The {@link SettingsFragment} to run.
     */
    void startSettings(Context context, @SettingsFragment int settingsFragment);

    /**
     * Starts a specific settings fragment. This can be used by code that does not supply its own
     * settings page, but instead needs to redirect the user to an appropriate page that is out of
     * reach.
     * This takes additional {@code addToBackStack} param to control fragment stack.
     * Note: unlike {@code Class<?> fragment} variations, this does not support {@code
     * fragmentArgs}, because it is (sometimes) automatically derived from {@code context} and
     * {@code settingsFragment}
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param settingsFragment The {@link SettingsFragment} to run.
     * @param addToBackStack If true, the fragment will be stack on the backstack of the fragment
     *     manager.
     */
    void startSettings(
            Context context, @SettingsFragment int settingsFragment, boolean addToBackStack);

    /**
     * Starts settings with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     */
    void startSettings(Context context, @Nullable Class<? extends Fragment> fragment);

    /**
     * Starts settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs);

    /**
     * Starts settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @param addToBackStack If true, the fragment will be stack on the backstack of the fragment
     *     manager.
     */
    void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack);

    /**
     * Starts settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @param addToBackStack If true, the fragment will be stack on the backstack of the fragment
     *     manager.
     * @param tag A tag used to identify the fragment transaction.
     */
    void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag);

    /**
     * Creates an intent for starting settings with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     */
    Intent createSettingsIntent(Context context, @Nullable Class<? extends Fragment> fragment);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @param addToBackStack If true, the fragment will be stack on the backstack of the fragment
     *     manager.
     */
    Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @param addToBackStack If true, the fragment will be stack on the backstack of the fragment
     *     manager.
     * @param tag A tag used to identify the fragment transaction.
     */
    Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    Intent createSettingsIntent(
            Context context, @SettingsFragment int fragment, @Nullable Bundle fragmentArgs);

    /**
     * Finishes the current settings.
     *
     * <p>Call this method when the user is done with the current settings page and should go back
     * to the previous page (e.g. selected a language from the language list).
     *
     * <p>If the given page is not the current one, or the page is already finished, this method
     * does nothing. In other words, this method is idempotent.
     *
     * <p>This method executes navigations asynchronously. It means that it is safe to call this
     * method on the UI thread in most cases, particularly even in the middle of executing fragment
     * transactions. On the other hand, you have to be careful when you want to go back multiple
     * pages using this method; it may not work as you expect to call this method multiple times in
     * a row because the subsequent method calls are ignored due to fragment mismatch. Use {@link
     * executePendingNavigations} to synchronously execute pending navigations to work around this
     * problem.
     *
     * @param fragment The expected current fragment.
     */
    void finishCurrentSettings(Fragment fragment);

    /**
     * Executes pending navigations immediately.
     *
     * <p>See {@link finishCurrentSettings} for a valid use case of this method.
     *
     * @param activity The settings activity.
     */
    void executePendingNavigations(Activity activity);

    /**
     * Forces the SettingsNavigation to use SettingsActivity, even if it would normally open
     * settings in a tab. TODO(crbug.com/521895796): Remove this after collecting data from canary
     * channel.
     *
     * @param value Whether to use SettingsActivity for testing.
     */
    void setUseSettingsActivityForTesting(boolean value);
}
