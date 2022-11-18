// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.content_public.browser.WebContents;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public AssistantOnboardingHelper createOnboardingHelper(
            WebContents webContents, AssistantDependencies dependencies) {
        return new AssistantOnboardingHelperImpl(webContents, dependencies);
    }
}
