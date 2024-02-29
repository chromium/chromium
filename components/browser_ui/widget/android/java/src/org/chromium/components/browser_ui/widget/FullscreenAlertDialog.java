// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.Toolbar;

import org.chromium.base.BuildInfo;
import org.chromium.components.browser_ui.util.AutomotiveUtils;

/**
 * Full screen AlertDialog in Clank
 *
 * This class will automatically add the back button toolbar to automotive devices in full screen
 * AlertDialogs.
 */
public class FullscreenAlertDialog extends AlertDialog {
    private Context mContext;
    private Toolbar mAutomotiveToolbar;

    public FullscreenAlertDialog(@NonNull Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mContext = context;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setAutomotiveToolbarBackButtonAction();
    }

    @Override
    public void setView(View view) {
        if (BuildInfo.getInstance().isAutomotive) {
            View automotiveLayout =
                    LayoutInflater.from(mContext)
                            .inflate(
                                    AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(
                                            mContext),
                                    null);
            ((ViewGroup) automotiveLayout).addView(view);
            mAutomotiveToolbar = automotiveLayout.findViewById(R.id.back_button_toolbar);
            super.setView(automotiveLayout);
        } else {
            super.setView(view);
        }
    }

    @Override
    public void setView(
            View view,
            int viewSpacingLeft,
            int viewSpacingTop,
            int viewSpacingRight,
            int viewSpacingBottom) {
        if (BuildInfo.getInstance().isAutomotive) {
            MarginLayoutParams params = (MarginLayoutParams) view.getLayoutParams();
            params.setMargins(viewSpacingLeft, viewSpacingTop, viewSpacingRight, viewSpacingBottom);
            ViewGroup automotiveLayout =
                    (ViewGroup)
                            LayoutInflater.from(mContext)
                                    .inflate(
                                            AutomotiveUtils
                                                    .getAutomotiveLayoutWithBackButtonToolbar(
                                                            mContext),
                                            null);
            automotiveLayout.addView(view, params);
            mAutomotiveToolbar = automotiveLayout.findViewById(R.id.back_button_toolbar);
            super.setView(automotiveLayout);
        } else {
            super.setView(
                    view, viewSpacingLeft, viewSpacingTop, viewSpacingRight, viewSpacingBottom);
        }
    }

    private void setAutomotiveToolbarBackButtonAction() {
        if (mAutomotiveToolbar != null) {
            mAutomotiveToolbar.setNavigationOnClickListener(
                    backButtonClick -> {
                        this.getOnBackPressedDispatcher().onBackPressed();
                    });
        }
    }

    public static class Builder extends AlertDialog.Builder {
        private Context mContext;
        private AlertDialog mAlertDialog;
        private Toolbar mAutomotiveToolbar;

        public Builder(@NonNull Context context) {
            super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
            mContext = context;
        }

        @Override
        public Builder setView(int layoutResId) {
            if (BuildInfo.getInstance().isAutomotive) {
                View automotiveLayout =
                        LayoutInflater.from(mContext)
                                .inflate(
                                        AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(
                                                mContext),
                                        null);
                mAutomotiveToolbar = automotiveLayout.findViewById(R.id.back_button_toolbar);
                ViewStub stub = automotiveLayout.findViewById(R.id.original_layout);
                stub.setLayoutResource(layoutResId);
                stub.inflate();
                super.setView(automotiveLayout);
            } else {
                super.setView(layoutResId);
            }
            return this;
        }

        @Override
        public Builder setView(View view) {
            if (BuildInfo.getInstance().isAutomotive) {
                ViewGroup automotiveLayout =
                        (ViewGroup)
                                LayoutInflater.from(mContext)
                                        .inflate(
                                                AutomotiveUtils
                                                        .getAutomotiveLayoutWithBackButtonToolbar(
                                                                mContext),
                                                null);
                mAutomotiveToolbar = automotiveLayout.findViewById(R.id.back_button_toolbar);
                automotiveLayout.addView(
                        view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                super.setView(automotiveLayout);
            } else {
                super.setView(view);
            }
            return this;
        }

        @Override
        public AlertDialog create() {
            mAlertDialog = super.create();
            if (mAutomotiveToolbar != null) {
                mAutomotiveToolbar.setNavigationOnClickListener(
                        backButtonClick -> {
                            mAlertDialog.getOnBackPressedDispatcher().onBackPressed();
                        });
            }
            return mAlertDialog;
        }
    }
}
