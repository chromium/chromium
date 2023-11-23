// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.DialogPreference;
import androidx.preference.PreferenceViewHolder;

/** Dialog that prompts the user to clear website storage on the device. */
public class ClearWebsiteStorage extends DialogPreference {
    Context mContext;

    // The host to show in the dialog.
    String mHost;

    // Whether to warn that apps will also be deleted.
    boolean mClearingApps;

    // Whether the dialog is for a group of sites.
    boolean mIsGroup;

    public ClearWebsiteStorage(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        initialize(context);
    }

    public ClearWebsiteStorage(Context context, AttributeSet attrs) {
        super(context, attrs);
        initialize(context);
    }

    private void initialize(Context context) {
        setDialogLayoutResource(R.layout.clear_data_dialog);
        mContext = context;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        assert mHost != null;
        super.onBindViewHolder(holder);

        int resourceId;
        if (!mIsGroup) {
            resourceId =
                    mClearingApps
                            ? R.string.webstorage_delete_data_dialog_message_single_with_app
                            : R.string.webstorage_delete_data_dialog_message_single;
        } else {
            resourceId =
                    mClearingApps
                            ? R.string.webstorage_delete_data_dialog_message_group_with_app
                            : R.string.webstorage_delete_data_dialog_message_group;
        }

        setDialogMessage(mContext.getString(resourceId, mHost));
    }

    /**
     * Set the data to show in the dialog.
     * @param host The host to show in the dialog.
     * @param clearingApps True if there is one or more apps involved, whose data will be deleted.
     * @param isGroup True if the dialog is related to a group of sites.
     */
    public void setDataForDisplay(String host, boolean clearingApps, boolean isGroup) {
        mHost = host;
        mClearingApps = clearingApps;
        mIsGroup = isGroup;
    }

    /**
     * Returns the string resource id to use to explain that the user will be signed out.
     * @param isGroup True if the dialog is related to a group of sites.
     */
    public static int getSignedOutText(boolean isGroup) {
        return isGroup
                ? R.string.webstorage_clear_data_dialog_sign_out_group_message
                : R.string.webstorage_clear_data_dialog_sign_out_message;
    }

    /** Returns the string resource id to use to explain what happens with offline files. */
    public static int getOfflineText() {
        return R.string.webstorage_delete_data_dialog_offline_message;
    }
}
