// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
@NullMarked
public class SearchEnginesFeatureUtils {

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
     * Whether the SearchEngineChoiceService should be powered by a fake backend. This avoids having
     * dependencies on other device apps/components to test the feature.
     *
     * <p>This can be controlled by starting chrome with the <code>
     * --enable-choice-apis-fake-backend</code> command line flag.
     *
     * <p>In "fake backend" mode, if the "ClayBlocking" feature's "dialog_timeout_millis" param is
     * set, it is also used to simulate a long-running backend query. It will respond that blocking
     * is required after {@code min(3000, paramValue("dialog_timeout_millis"))} milliseconds.
     */
    public boolean isChoiceApisFakeBackendEnabled() {
        return CommandLine.getInstance().hasSwitch(ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
    }

    /** Number of times a failed backend call will be retried. */
    public int choiceApisConnectionMaxRetries() {
        return CHOICE_APIS_CONNECTION_MAX_RETRIES;
    }

    // Do not instantiate this class.
    private SearchEnginesFeatureUtils() {}
}
