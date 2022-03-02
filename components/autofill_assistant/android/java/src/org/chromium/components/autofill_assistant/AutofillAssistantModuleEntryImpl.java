// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public AssistantOnboardingHelper createOnboardingHelper(
            WebContents webContents, AssistantDependencies dependencies) {
        return new AssistantOnboardingHelperImpl(webContents, dependencies);
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            Supplier<WebContents> webContentsSupplier,
            AssistantStaticDependencies staticDependencies) {
        return new AutofillAssistantActionHandlerImpl(
                new OnboardingCoordinatorFactory(context, bottomSheetController,
                        staticDependencies.getBrowserContext(), browserControlsFactory, rootView,
                        staticDependencies.getAccessibilityUtil(),
                        staticDependencies.createInfoPageUtil()),
                webContentsSupplier, staticDependencies);
    }
}
