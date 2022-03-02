// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.onboarding;

import android.content.Context;
import android.view.View;

import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.util.AccessibilityUtil;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Onboarding coordinator factory which facilitates the creation of onboarding coordinators.
 */
public class OnboardingCoordinatorFactory {
    // See details and variant breakdown in b/209561294.
    private static final String SPLIT_ONBOARDING_VARIANT_A_EXPERIMENT_ID = "4702489";
    private static final String SPLIT_ONBOARDING_VARIANT_B_EXPERIMENT_ID = "4702490";

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserContextHandle mBrowserContext;
    private final AssistantBrowserControlsFactory mBrowserControlsFactory;
    private final View mRootView;
    private final AccessibilityUtil mAccessibilityUtil;
    private final AssistantInfoPageUtil mInfoPageUtil;

    public OnboardingCoordinatorFactory(Context context,
            BottomSheetController bottomSheetController, BrowserContextHandle browserContext,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            AccessibilityUtil accessibilityUtil, AssistantInfoPageUtil infoPageUtil) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBrowserContext = browserContext;
        mBrowserControlsFactory = browserControlsFactory;
        mRootView = rootView;
        mAccessibilityUtil = accessibilityUtil;
        mInfoPageUtil = infoPageUtil;
    }

    /**
     * Creates an onboarding coordinator ready to be shown in the bottom sheet.
     */
    public BaseOnboardingCoordinator createBottomSheetOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        List<String> experimentIdsList = Arrays.asList(experimentIds.split(","));
        if (experimentIdsList.contains(SPLIT_ONBOARDING_VARIANT_A_EXPERIMENT_ID)) {
            return new BottomSheetOnboardingWithPopupCoordinator(mBrowserContext, mInfoPageUtil,
                    experimentIds, parameters, mContext, mBottomSheetController,
                    mBrowserControlsFactory, mRootView,
                    mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
        } else if (experimentIdsList.contains(SPLIT_ONBOARDING_VARIANT_B_EXPERIMENT_ID)) {
            return new BottomSheetOnboardingWithPopupAndBubbleCoordinator(mBrowserContext,
                    mInfoPageUtil, experimentIds, parameters, mContext, mBottomSheetController,
                    mBrowserControlsFactory, mRootView,
                    mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
        }

        return new BottomSheetOnboardingCoordinator(mBrowserContext, mInfoPageUtil, experimentIds,
                parameters, mContext, mBottomSheetController, mBrowserControlsFactory, mRootView,
                mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
    }

    /**
     * Creates an onboarding coordinator that will appear as a standalong popup dialog.
     */
    public BaseOnboardingCoordinator createDialogOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        return new DialogOnboardingCoordinator(
                mBrowserContext, mInfoPageUtil, experimentIds, parameters, mContext);
    }
}
