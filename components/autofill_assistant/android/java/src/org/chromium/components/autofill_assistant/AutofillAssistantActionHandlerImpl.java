// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.components.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.components.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A handler that provides Autofill Assistant actions for a specific activity.
 */
public class AutofillAssistantActionHandlerImpl implements AutofillAssistantActionHandler {
    private final OnboardingCoordinatorFactory mOnboardingCoordinatorFactory;
    private final AssistantStaticDependencies mStaticDependencies;
    private final Supplier<WebContents> mWebContentsSupplier;
    // Encapsulates access to {@link PrefService} pref keys for Autofill Assistant.
    private final AutofillAssistantPreferenceManager mPreferenceManager;

    public AutofillAssistantActionHandlerImpl(
            OnboardingCoordinatorFactory onboardingCoordinatorFactory,
            Supplier<WebContents> webContentsSupplier,
            AssistantStaticDependencies staticDependencies) {
        mOnboardingCoordinatorFactory = onboardingCoordinatorFactory;
        mWebContentsSupplier = webContentsSupplier;
        mStaticDependencies = staticDependencies;
        mPreferenceManager = new AutofillAssistantPreferenceManager(
                UserPrefs.get(mStaticDependencies.getBrowserContext()));
    }

    @Override
    public void fetchWebsiteActions(
            String userName, String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        if (!mPreferenceManager.getOnboardingConsent()) {
            callback.onResult(false);
            return;
        }
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(false);
            return;
        }

        client.fetchWebsiteActions(userName, experimentIds, toArgumentMap(arguments), callback);
    }

    @Override
    public boolean hasRunFirstCheck() {
        if (!mPreferenceManager.getOnboardingConsent()) {
            return false;
        }

        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) return false;
        return client.hasRunFirstCheck();
    }

    @Override
    public List<AutofillAssistantDirectAction> getActions() {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            return Collections.emptyList();
        }
        return client.getDirectActions();
    }

    @Override
    public void performOnboarding(
            String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        Map<String, String> parameters = toArgumentMap(arguments);
        BaseOnboardingCoordinator coordinator =
                mOnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                        experimentIds, parameters);
        coordinator.show(result -> {
            coordinator.hide();
            callback.onResult(result == AssistantOnboardingResult.ACCEPTED);
        });
    }

    @Override
    public void performAction(
            String name, String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(false);
            return;
        }

        Map<String, String> argumentMap = toArgumentMap(arguments);
        Callback<AssistantOverlayCoordinator> afterOnboarding = (overlayCoordinator) -> {
            callback.onResult(client.performDirectAction(
                    name, experimentIds, argumentMap, overlayCoordinator));
        };

        if (!mPreferenceManager.getOnboardingConsent()) {
            BaseOnboardingCoordinator coordinator =
                    mOnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                            experimentIds, argumentMap);
            coordinator.show(result -> {
                if (result != AssistantOnboardingResult.ACCEPTED) {
                    coordinator.hide();
                    callback.onResult(false);
                    return;
                }
                afterOnboarding.onResult(coordinator.transferControls());
            });
            return;
        }
        afterOnboarding.onResult(null);
    }

    @Override
    public void showFatalError() {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            return;
        }
        client.showFatalError();
    }

    @Override
    public boolean isSupervisedUser() {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            return false;
        }
        return client.isSupervisedUser();
    }

    /**
     * Returns a client for the current tab or {@code null} if there's no current tab or the current
     * tab doesn't have an associated browser content.
     */
    @Nullable
    private AutofillAssistantClient getOrCreateClient() {
        ThreadUtils.assertOnUiThread();

        WebContents webContents = mWebContentsSupplier.get();
        Activity activity = getActivityFromWebContents(webContents);
        if (webContents == null || activity == null) {
            return null;
        }

        return AutofillAssistantClient.createForWebContents(
                webContents, mStaticDependencies.createDependencies(activity));
    }

    /**
     * Looks up the Activity of the given web contents. This can be null. Should never be cached,
     * because web contents can change activities, e.g., when user selects "Open in Chrome" menu
     * item.
     *
     * @param webContents The web contents for which to lookup the Activity.
     * @return Activity currently related to webContents. Could be <c>null</c> and could change,
     *         therefore do not cache.
     */
    @Nullable
    private static Activity getActivityFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        return window.getActivity().get();
    }

    /** Extracts string arguments from a bundle. */
    private Map<String, String> toArgumentMap(Bundle bundle) {
        Map<String, String> map = new HashMap<>();
        for (String key : bundle.keySet()) {
            String value = bundle.getString(key);
            if (value != null) {
                map.put(key, value);
            }
        }
        return map;
    }
}
