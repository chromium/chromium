// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.autofill_assistant.metrics.DropOutReason;
import org.chromium.components.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.components.autofill_assistant.strings.IntentStrings;
import org.chromium.content_public.browser.WebContents;

/**
 * Records user actions and histograms related to Autofill Assistant.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
public class AutofillAssistantMetrics {
    /**
     * Records the reason for a drop out.
     */
    public static void recordDropOut(@DropOutReason int reason, String intent) {
        String histogramSuffix = getHistogramSuffixForIntent(intent);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason." + histogramSuffix, reason,
                DropOutReason.MAX_VALUE + 1);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason", reason, DropOutReason.MAX_VALUE + 1);
    }

    /**
     * Records the feature module installation action.
     */
    public static void recordFeatureModuleInstallation(@FeatureModuleInstallation int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.FeatureModuleInstallation", metric,
                FeatureModuleInstallation.MAX_VALUE + 1);
    }

    /**
     * Returns whether {@code webContents} are non-null and valid. Invalid
     * webContents will cause a failed DCHECK when attempting to report UKM metrics.
     */
    private static boolean areWebContentsValid(@Nullable WebContents webContents) {
        return webContents != null && !webContents.isDestroyed();
    }

    /**
     * Returns histogram suffix for given intent.
     */
    private static String getHistogramSuffixForIntent(@Nullable String intent) {
        if (intent == null) {
            // Intent is not set.
            return "NotSet";
        }
        switch (intent) {
            case IntentStrings.BUY_MOVIE_TICKET:
                return "BuyMovieTicket";
            case IntentStrings.CHROME_FAST_CHECKOUT:
                return "ChromeFastCheckout";
            case IntentStrings.FLIGHTS_CHECKIN:
                return "FlightsCheckin";
            case IntentStrings.FOOD_ORDERING:
                return "FoodOrdering";
            case IntentStrings.FOOD_ORDERING_DELIVERY:
                return "FoodOrderingDelivery";
            case IntentStrings.FOOD_ORDERING_PICKUP:
                return "FoodOrderingPickup";
            case IntentStrings.PASSWORD_CHANGE:
                return "PasswordChange";
            case IntentStrings.RENT_CAR:
                return "RentCar";
            case IntentStrings.SHOPPING:
                return "Shopping";
            case IntentStrings.SHOPPING_ASSISTED_CHECKOUT:
                return "ShoppingAssistedCheckout";
            case IntentStrings.TELEPORT:
                return "Teleport";
        }
        return "UnknownIntent";
    }
}
