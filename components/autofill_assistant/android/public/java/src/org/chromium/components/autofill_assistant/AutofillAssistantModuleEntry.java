// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

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
}
