// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.components.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.components.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.autofill_assistant.trigger_scripts.AssistantTriggerScriptBridge;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Concrete implementation of the OnboardingHelper interface.
 */
@JNINamespace("autofill_assistant")
public class AssistantOnboardingHelperImpl implements AssistantOnboardingHelper {
    private final OnboardingCoordinatorFactory mOnboardingCoordinatorFactory;
    private final AssistantTriggerScriptBridge mTriggerScriptBridge;

    /**
     * The currently shown onboarding coordinator, if any. Only set while the onboarding is shown.
     */
    private @Nullable BaseOnboardingCoordinator mOnboardingCoordinator;

    /** The most recently shown onboarding overlay coordinator, if any. */
    private @Nullable AssistantOverlayCoordinator mOnboardingOverlayCoordinator;

    AssistantOnboardingHelperImpl(WebContents webContents, AssistantDependencies dependencies) {
        mOnboardingCoordinatorFactory = new OnboardingCoordinatorFactory(dependencies.getActivity(),
                dependencies.getBottomSheetController(), dependencies.getBrowserContext(),
                dependencies.createBrowserControlsFactory(), dependencies.getRootView(),
                dependencies.getAccessibilityUtil(), dependencies.createInfoPageUtil());
        mTriggerScriptBridge = new AssistantTriggerScriptBridge(webContents, dependencies);
    }

    /**
     * Shows the onboarding to the user. Also takes ownership of the shown overlay coordinator after
     * the onboarding is finished such that the regular startup can reuse it.
     */
    @Override
    public void showOnboarding(boolean useDialogOnboarding, String experimentIds,
            Map<String, String> parameters, Callback<Integer> callback) {
        hideOnboarding();
        if (useDialogOnboarding) {
            mOnboardingCoordinator =
                    mOnboardingCoordinatorFactory.createDialogOnboardingCoordinator(
                            experimentIds, parameters);
        } else {
            mOnboardingCoordinator =
                    mOnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                            experimentIds, parameters);
        }

        mOnboardingCoordinator.show(result -> {
            // Note: only transfer the controls in the ACCEPTED case, as it will prevent
            // the bottom sheet from hiding after the callback is done.
            if (result == AssistantOnboardingResult.ACCEPTED) {
                mOnboardingOverlayCoordinator = mOnboardingCoordinator.transferControls();
            }
            callback.onResult(result);
        });
    }

    @Override
    public void hideOnboarding() {
        if (mOnboardingCoordinator != null) {
            mOnboardingCoordinator.hide();
            mOnboardingCoordinator = null;
        }
    }

    /**
     * Transfers ownership of the overlay coordinator shown during the most recent onboarding, if
     * any.
     */
    @CalledByNative
    public @Nullable AssistantOverlayCoordinator transferOnboardingOverlayCoordinator() {
        AssistantOverlayCoordinator overlayCoordinator = mOnboardingOverlayCoordinator;
        mOnboardingOverlayCoordinator = null;
        return overlayCoordinator;
    }
}
