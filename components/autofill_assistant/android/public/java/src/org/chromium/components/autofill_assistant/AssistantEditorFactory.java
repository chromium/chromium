// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Factory for creating editors. Implementations might differ depending on where Autofill
 * Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantEditorFactory {
    AssistantContactEditor createContactEditor(WebContents webContents, Activity activity,
            boolean requestName, boolean requestPhone, boolean requestEmail,
            boolean shouldStoreChanges);

    AssistantContactEditor createAccountEditor(Activity activity, WindowAndroid windowAndroid,
            String accountEmail, boolean requestEmail, boolean requestPhone);

    AssistantAddressEditor createAddressEditor(
            WebContents webContents, Activity activity, boolean shouldStoreChanges);

    AssistantAddressEditor createGmsAddressEditor(Activity activity, WindowAndroid windowAndroid,
            String accountEmail, byte[] initializeAddressCollectionParams);

    AssistantPaymentInstrumentEditor createPaymentInstrumentEditor(WebContents webContents,
            Activity activity, List<String> supportedCardNetworks, boolean shouldStoreChanges);

    AssistantPaymentInstrumentEditor createGmsPaymentInstrumentEditor(Activity activity,
            WindowAndroid windowAndroid, String accountEmail, byte[] addInstrumentactionToken);
}
