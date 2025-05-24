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
import android.widget.FrameLayout;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.Toolbar;

import org.chromium.base.BuildInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;
import org.chromium.components.browser_ui.util.AutomotiveUtils;

/**
 * Full screen AlertDialog in Clank
 *
 * <p>This class will automatically add the back button toolbar to automotive devices in full screen
 * AlertDialogs.
 */
@NullMarked
public class FullscreenAlertDialog extends AlertDialog {
    private final Context mContext;
    private final boolean mShouldPadForContent;
    private @Nullable Toolbar mAutomotiveToolbar;
    private @Nullable EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayout;

    /**
     * Create a fullscreen AlertDialog.
     *
     * @param context Activity context used to show the dialog.
     * @param shouldPadForContent Whether the content should be padded to avoid window insets.
     */
    // Note: shouldPadForContent should always be true once Chrome is drawing edge to edge.
    public FullscreenAlertDialog(Context context, boolean shouldPadForContent) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mContext = context;
        mShouldPadForContent = shouldPadForContent;
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
        } else if (mShouldPadForContent) {
            mEdgeToEdgeLayout = initEdgeToEdgeLayoutCoordinator(mContext);
            super.setView(mEdgeToEdgeLayout.wrapContentView(view));
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
        } else if (mShouldPadForContent) {
            MarginLayoutParams params = (MarginLayoutParams) view.getLayoutParams();
            params.setMargins(viewSpacingLeft, viewSpacingTop, viewSpacingRight, viewSpacingBottom);
            mEdgeToEdgeLayout = initEdgeToEdgeLayoutCoordinator(mContext);
            super.setView(mEdgeToEdgeLayout.wrapContentView(view, params));
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
        private final Context mContext;
        private final @Nullable EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayout;
        private @Nullable Toolbar mAutomotiveToolbar;

        /**
         * Create a builder for FullscreenAlertDialog.
         *
         * @param context The activity context
         * @param shouldPadForContent Whether the content should be padded to avoid window insets.
         */
        // Note: shouldPadForContent should always be true once Chrome is drawing edge to edge.
        public Builder(Context context, boolean shouldPadForContent) {
            super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
            mContext = context;

            if (shouldPadForContent && !BuildInfo.getInstance().isAutomotive) {
                mEdgeToEdgeLayout = initEdgeToEdgeLayoutCoordinator(mContext);
            } else {
                mEdgeToEdgeLayout = null;
            }
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
            } else if (mEdgeToEdgeLayout != null) {
                FrameLayout baseLayout = new FrameLayout(mContext);
                super.setView(mEdgeToEdgeLayout.wrapContentView(baseLayout));
                LayoutInflater.from(mContext)
                        .inflate(layoutResId, baseLayout, /* attachToRoot= */ true);
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
            } else if (mEdgeToEdgeLayout != null) {
                super.setView(mEdgeToEdgeLayout.wrapContentView(view));
            } else {
                super.setView(view);
            }
            return this;
        }

        @Override
        public AlertDialog create() {
            AlertDialog dialog = super.create();
            if (mAutomotiveToolbar != null) {
                mAutomotiveToolbar.setNavigationOnClickListener(
                        backButtonClick -> {
                            dialog.getOnBackPressedDispatcher().onBackPressed();
                        });
            }
            return dialog;
        }
    }

    private static EdgeToEdgeLayoutCoordinator initEdgeToEdgeLayoutCoordinator(Context context) {
        // TODO(crbug.com/401075913): Color sys bars according to dialog content.
        return new EdgeToEdgeLayoutCoordinator(context, null);
    }
}
