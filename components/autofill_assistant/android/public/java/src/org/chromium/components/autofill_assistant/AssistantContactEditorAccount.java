// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.components.autofill_assistant.user_data.GmsIntegrator;
import org.chromium.ui.base.WindowAndroid;

/**
 * Editor for contact information in Chrome/WebLayer using a GMS intent.
 */
public class AssistantContactEditorAccount implements AssistantContactEditor {
    // Enums defined in resource_id.proto of AccountSettings.
    private static final int SCREEN_ID_PERSONAL_INFO_SCREEN = 10003; // All info.
    private static final int SCREEN_ID_MISC_CONTACT_EMAIL_SCREEN = 501;
    private static final int SCREEN_ID_PRIVACY_PHONE_SCREEN = 204;

    private final WindowAndroid mWindowAndroid;
    private final GmsIntegrator mGmsIntegrator;
    private final boolean mRequestEmail;
    private final boolean mRequestPhone;

    public AssistantContactEditorAccount(Activity activity, WindowAndroid windowAndroid,
            String accountEmail, boolean requestEmail, boolean requestPhone) {
        mWindowAndroid = windowAndroid;
        mGmsIntegrator = new GmsIntegrator(accountEmail, activity);
        mRequestEmail = requestEmail;
        mRequestPhone = requestPhone;
    }

    /**
     * Edit the user's personal information. If the email is requested, the editor opens to the
     * contact email screen. If phone number is requested, the editor opens to the phone screen.
     * It is not allowed to request email and phone at the same time! If neither is requested -
     * e.g. we're looking for name only - the editor opens to the main view, where all information
     * is available.
     *
     * @param oldItem The item to be edited, can be null in which case a new item is created.
     * @param doneCallback Called after the editor is closed, assuming that the item has been
     *                     successfully edited. The callback will be called with the
     *                     {@code oldItem} which can be null. The list of new items needs to be
     *                     requested.
     * @param cancelCallback Only called if the intent failed to be launched.
     */
    @Override
    public void createOrEditItem(@Nullable ContactModel oldItem,
            Callback<ContactModel> doneCallback, Callback<ContactModel> cancelCallback) {
        Callback<Boolean> callback = success -> {
            if (success) {
                doneCallback.onResult(oldItem);
            } else {
                cancelCallback.onResult(oldItem);
            }
        };

        int screenId;
        if (mRequestEmail) {
            assert !mRequestPhone;
            screenId = SCREEN_ID_MISC_CONTACT_EMAIL_SCREEN;
        } else if (mRequestPhone) {
            assert !mRequestEmail;
            screenId = SCREEN_ID_PRIVACY_PHONE_SCREEN;
        } else {
            screenId = SCREEN_ID_PERSONAL_INFO_SCREEN;
        }
        mGmsIntegrator.launchAccountIntent(screenId, mWindowAndroid, callback);
    }
}
