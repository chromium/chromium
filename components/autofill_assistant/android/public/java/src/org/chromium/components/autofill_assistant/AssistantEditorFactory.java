// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/**
 * Factory for creating editors. Implementations might differ depending on where Autofill
 * Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantEditorFactory {
    @Nullable
    AssistantContactEditor createContactEditor(WebContents webContents, Activity activity,
            boolean requestName, boolean requestPhone, boolean requestEmail,
            boolean shouldStoreChanges);

    @Nullable
    AssistantAddressEditor createAddressEditor(
            WebContents webContents, Activity activity, boolean shouldStoreChanges);

    @Nullable
    AssistantPaymentInstrumentEditor createPaymentInstrumentEditor(WebContents webContents,
            Activity activity, List<String> supportedCardNetworks, boolean shouldStoreChanges);
}
