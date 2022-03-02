// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.content_public.browser.WebContents;

/**
 * Interface between base module and assistant DFM.
 */
// TODO(fga): Figure out how to do this.
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.components.autofill_assistant.AutofillAssistantModuleEntryImpl")
public interface AutofillAssistantModuleEntry {
    /**
     * Creates a concrete {@code AssistantOnboardingHelper} object. Its contents are opaque to
     * the outside of the module.
     */
    AssistantOnboardingHelper createOnboardingHelper(
            WebContents webContents, AssistantDependencies dependencies);

    /**
     * Returns a {@link AutofillAssistantActionHandler} instance tied to the activity owning the
     * given bottom sheet, and scrim view.
     *
     * @param context activity context
     * @param bottomSheetController bottom sheet controller instance of the activity
     * @param browserControlsFactory factory for providing browser controls state
     * @param rootView root view of the activity
     * @param webContentsSupplier supplier of the current WebContents
     * @param staticDependencies used to create platform-specific dependencies
     */
    AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            Supplier<WebContents> webContentsSupplier,
            AssistantStaticDependencies staticDependencies);
}
