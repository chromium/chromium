// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.components.autofill_assistant.user_data.GmsIntegrator;
import org.chromium.ui.base.WindowAndroid;

/**
 * Editor for addresses in Chrome/WebLayer using a GMS intent.
 */
public class AssistantAddressEditorGms implements AssistantAddressEditor {
    private final WindowAndroid mWindowAndroid;
    private final GmsIntegrator mGmsIntegrator;
    private final byte[] mInitializeAddressCollectionParams;

    public AssistantAddressEditorGms(Activity activity, WindowAndroid windowAndroid,
            String accountEmail, byte[] initializeAddressCollectionParams) {
        mWindowAndroid = windowAndroid;
        mGmsIntegrator = new GmsIntegrator(accountEmail, activity);
        mInitializeAddressCollectionParams = initializeAddressCollectionParams;
    }

    /**
     * Edit the user's addresses.
     *
     * @param oldItem The item to be edited, can be null in which case a new item is created.
     * @param doneCallback Called after the editor is closed, assuming that the item has been
     *                     successfully edited. The callback will be called with the
     *                     {@code oldItem} which can be null. The list of new items needs to be
     *                     requested.
     * @param cancelCallback Only called if the intent failed to be launched.
     */
    @Override
    public void createOrEditItem(AddressModel oldItem, Callback<AddressModel> doneCallback,
            Callback<AddressModel> cancelCallback) {
        Callback<Boolean> callback = success -> {
            if (success) {
                doneCallback.onResult(oldItem);
            } else {
                cancelCallback.onResult(oldItem);
            }
        };

        if (oldItem == null) {
            mGmsIntegrator.launchAddressCollectionIntent(
                    mInitializeAddressCollectionParams, mWindowAndroid, callback);
        } else {
            byte[] editToken = oldItem.getEditToken();
            if (editToken == null) {
                assert false; // Should never happen!
                cancelCallback.onResult(oldItem);
                return;
            }

            mGmsIntegrator.launchAddressCollectionIntent(editToken, mWindowAndroid, callback);
        }
    }
}
