// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceDialogFragmentCompat;

import org.chromium.base.Callback;
import org.chromium.ui.base.ViewUtils;

/**
 * The fragment used to display the clear website storage confirmation dialog.
 */
public class ClearWebsiteStorageDialog extends PreferenceDialogFragmentCompat {
    public static final String TAG = "ClearWebsiteStorageDialog";
    private static final String SHOULD_SHOW_AD_PERSONALIZATION_ROW =
            "should_show_ad_personalization_row";

    private static Callback<Boolean> sCallback;

    // The view containing the dialog ui elements.
    private View mDialogView;

    private boolean mShouldShowAdPersonalizationRow;

    public static ClearWebsiteStorageDialog newInstance(Preference preference,
            Callback<Boolean> callback, boolean shouldShowAdPersonalizationRow) {
        ClearWebsiteStorageDialog fragment = new ClearWebsiteStorageDialog();
        sCallback = callback;
        Bundle bundle = new Bundle(2);
        bundle.putString(PreferenceDialogFragmentCompat.ARG_KEY, preference.getKey());
        bundle.putBoolean(SHOULD_SHOW_AD_PERSONALIZATION_ROW, shouldShowAdPersonalizationRow);
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    protected void onBindDialogView(View view) {
        mDialogView = view;

        TextView signedOutView = view.findViewById(R.id.signed_out_text);
        TextView offlineTextView = view.findViewById(R.id.offline_text);
        signedOutView.setText(ClearWebsiteStorage.getSignedOutText());
        offlineTextView.setText(ClearWebsiteStorage.getOfflineText());
        var shouldShowAdPersonalizationRow =
                getArguments().getBoolean(SHOULD_SHOW_AD_PERSONALIZATION_ROW, false);
        if (shouldShowAdPersonalizationRow) {
            RelativeLayout adDataRow = mDialogView.findViewById(R.id.ad_personalization);
            adDataRow.setVisibility(View.VISIBLE);
        }

        super.onBindDialogView(view);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mDialogView != null) {
            // When the device switches to multi-window in landscape mode, the height of the
            // offlineTextView is not calculated correctly (its height gets truncated) and a layout
            // pass is needed to fix it. See https://crbug.com/1072922.
            mDialogView.getHandler().post(() -> {
                ViewUtils.requestLayout(
                        mDialogView, "ClearWebsiteStorageDialog.onConfigurationChanged Runnable");
            });
        }
    }

    @Override
    public void onDialogClosed(boolean positiveResult) {
        sCallback.onResult(positiveResult);
    }
}
