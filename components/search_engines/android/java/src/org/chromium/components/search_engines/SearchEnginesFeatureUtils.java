// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.SearchEngineCountryDelegate.ChoiceScope;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
@NullMarked
public class SearchEnginesFeatureUtils {
    private static final String TAG = "SearchEnginesFeat";

    static final String ENABLE_CHOICE_APIS_DEBUG_SWITCH = "enable-choice-apis-debug";
    static final String ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH = "enable-choice-apis-fake-backend";

    /**
     * Millis after the OS default browser choice has been made during which Chrome should not offer
     * the user to set Chrome as their default browser. `Integer.MAX_VALUE` should give us a bit
     * more than 24 days, enough for our purposes,
     */
    public static final int CHOICE_DIALOG_DEFAULT_BROWSER_PROMO_SUPPRESSED_MILLIS =
            24 * 60 * 60 * 1000;

    private static final int CHOICE_APIS_CONNECTION_MAX_RETRIES = 2;
    private static @Nullable SearchEnginesFeatureUtils sInstance;

    public static SearchEnginesFeatureUtils getInstance() {
        if (sInstance == null) {
            sInstance = new SearchEnginesFeatureUtils();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(SearchEnginesFeatureUtils instance) {
        var oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    /**
     * Whether verbose logs associated with device choice APIs should be enabled.
     *
     * <p>This can be controlled by starting chrome with the <code>--enable-choice-apis-debug</code>
     * command line flag.
     */
    public boolean isChoiceApisDebugEnabled() {
        return CommandLine.getInstance().hasSwitch(ENABLE_CHOICE_APIS_DEBUG_SWITCH);
    }

    /**
     * Returns the {@link ChoiceScope} to simulate with a fake backend, or {@code null} if the fake
     * backend is not enabled. This avoids having dependencies on other device apps/components to
     * test the feature.
     *
     * <p>This can be controlled by starting Chrome with the <code>
     * --enable-choice-apis-fake-backend</code> command line flag:
     *
     * <ul>
     *   <li><code>--enable-choice-apis-fake-backend</code> or <code>
     *       --enable-choice-apis-fake-backend=0</code>: {@link ChoiceScope#DEFAULT}
     *   <li><code>--enable-choice-apis-fake-backend=1</code>: {@link
     *       ChoiceScope#CHROME_SEARCH_ONLY} (note: the {@code ChromeSearchOnlyClayScope} feature
     *       flag must also be enabled for the dialog UI to display search-only strings)
     * </ul>
     *
     * <p>The "fake backend" mode simulates a long-running backend query. It will respond that
     * blocking is required after `FakeSearchEngineCountryDelegate#CHOICE_REQUIRED_DELAY_MS`.
     */
    public @Nullable @ChoiceScope Integer choiceApisFakeBackendScope() {
        CommandLine commandLine = CommandLine.getInstance();
        if (!commandLine.hasSwitch(ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH)) {
            return null;
        }
        String value = commandLine.getSwitchValue(ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
        if (TextUtils.isEmpty(value)) {
            return ChoiceScope.DEFAULT;
        }
        try {
            int parsed = Integer.parseInt(value);
            switch (parsed) {
                case ChoiceScope.DEFAULT:
                    return ChoiceScope.DEFAULT;
                case ChoiceScope.CHROME_SEARCH_ONLY:
                    return ChoiceScope.CHROME_SEARCH_ONLY;
            }
        } catch (NumberFormatException ignored) {
        }
        Log.w(
                TAG,
                "Unsupported --%s value '%s'; falling back to ChoiceScope.DEFAULT.",
                ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH,
                value);
        return ChoiceScope.DEFAULT;
    }

    /** Number of times a failed backend call will be retried. */
    public int choiceApisConnectionMaxRetries() {
        return CHOICE_APIS_CONNECTION_MAX_RETRIES;
    }

    // Do not instantiate this class.
    private SearchEnginesFeatureUtils() {}
}
