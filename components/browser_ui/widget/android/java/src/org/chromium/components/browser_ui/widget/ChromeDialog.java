// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.activity.ComponentDialog;
import androidx.annotation.LayoutRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.widget.Toolbar;

import org.chromium.base.BuildInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ImmutableWeakReference;

/**
 * Dialog class in Chrome
 *
 * <p>This class will automatically add the back button toolbar to automotive devices in full screen
 * Dialogs. This class will also automatically wrap Dialogs with the EdgeToEdgeBaseLayout in order
 * to ensure correct padding for bottom and top insets.
 */
@NullMarked
public class ChromeDialog extends ComponentDialog {
    private final boolean mIsFullScreen;
    private final Activity mActivity;
    @Nullable private InsetObserver mInsetObserver;
    @Nullable private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private final boolean mShouldPadForWindowInsets;

    /**
     * Constructs the dialog class in Chrome.
     *
     * @param activity The base activity.
     * @param themeResId Theme resource.
     * @param shouldPadForWindowInsets Value of EdgeToEdgeEverywhere flag. Determines whether the
     *     dialog will be wrapped with the EdgeToEdgeBaseLayout.
     */
    public ChromeDialog(
            Activity activity, @StyleRes int themeResId, boolean shouldPadForWindowInsets) {
        super(activity, themeResId);
        mActivity = activity;
        if (themeResId == R.style.ThemeOverlay_BrowserUI_Fullscreen) {
            mIsFullScreen = true;
        } else {
            mIsFullScreen = false;
        }
        mShouldPadForWindowInsets = shouldPadForWindowInsets;
        if (mShouldPadForWindowInsets && getWindow() != null) {
            mInsetObserver =
                    new InsetObserver(
                            new ImmutableWeakReference<>(getWindow().getDecorView().getRootView()),
                            // Keyboard overlay mode is enabled by default and is currently only
                            // relevant to the DeferredImeWindowInsetApplicationCallback.
                            /* enableKeyboardOverlayMode= */ true);
        }
        // Currently, only the EdgeToEdgeLayoutCoordinator is listening to this InsetObserver,
        // and that class can handle cases with a null Window / null InsetObserver. Before
        // adding any new Insets observers / consumers, ensure that the window is not null / the
        // InsetObserver is reliably being created.
        // TODO(crbug.com/402995103): Clean up getWindow() null check assert.
        assert getWindow() != null : "Checking if there are cases when getWindow() is null";
    }

    @Override
    public void setContentView(@LayoutRes int layoutResID) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(
                    AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(mActivity));
            setAutomotiveToolbarBackButtonAction();
            ViewStub stub = findViewById(R.id.original_layout);
            stub.setLayoutResource(layoutResID);
            stub.inflate();
            // TODO(crbug.com/402226908): Add margins when for dialog when not in fullscreen
        } else if (mShouldPadForWindowInsets && mIsFullScreen) {
            super.setContentView(
                    ensureEdgeToEdgeLayoutCoordinator()
                            .wrapContentView(getLayoutInflater().inflate(layoutResID, null)));
        } else {
            super.setContentView(layoutResID);
        }
    }

    @Override
    public void setContentView(View view) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(
                    AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(mActivity));
            setAutomotiveToolbarBackButtonAction();
            LinearLayout linearLayout = findViewById(R.id.automotive_base_linear_layout);
            linearLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        } else if (mShouldPadForWindowInsets && mIsFullScreen) {
            super.setContentView(ensureEdgeToEdgeLayoutCoordinator().wrapContentView(view));
        } else {
            super.setContentView(view);
        }
    }

    @Override
    public void setContentView(View view, ViewGroup.@Nullable LayoutParams params) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(
                    AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(mActivity));
            setAutomotiveToolbarBackButtonAction();
            LinearLayout linearLayout = findViewById(R.id.automotive_base_linear_layout);
            linearLayout.addView(view, params);
        } else if (mShouldPadForWindowInsets && mIsFullScreen) {
            super.setContentView(ensureEdgeToEdgeLayoutCoordinator().wrapContentView(view, params));
        } else {
            super.setContentView(view, params);
        }
    }

    @Override
    public void addContentView(View view, ViewGroup.@Nullable LayoutParams params) {
        if (BuildInfo.getInstance().isAutomotive
                && mIsFullScreen
                && assumeNonNull(params).width == MATCH_PARENT
                && params.height == MATCH_PARENT) {
            ViewGroup automotiveLayout =
                    (ViewGroup)
                            LayoutInflater.from(mActivity)
                                    .inflate(
                                            AutomotiveUtils
                                                    .getAutomotiveLayoutWithBackButtonToolbar(
                                                            mActivity),
                                            null);
            super.addContentView(
                    automotiveLayout, new LinearLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
            setAutomotiveToolbarBackButtonAction();
            automotiveLayout.addView(view, params);
        } else if (mShouldPadForWindowInsets && mIsFullScreen) {
            super.addContentView(
                    ensureEdgeToEdgeLayoutCoordinator().wrapContentView(view, params), params);
        } else {
            super.addContentView(view, params);
        }
    }

    private void setAutomotiveToolbarBackButtonAction() {
        Toolbar backButtonToolbarForAutomotive = findViewById(R.id.back_button_toolbar);
        if (backButtonToolbarForAutomotive != null) {
            backButtonToolbarForAutomotive.setNavigationOnClickListener(
                    backButtonClick -> {
                        getOnBackPressedDispatcher().onBackPressed();
                    });
        }
    }

    private EdgeToEdgeLayoutCoordinator ensureEdgeToEdgeLayoutCoordinator() {
        if (mEdgeToEdgeLayoutCoordinator == null) {
            mEdgeToEdgeLayoutCoordinator =
                    new EdgeToEdgeLayoutCoordinator(mActivity, mInsetObserver);
            mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(
                    SemanticColorUtils.getDefaultBgColor(mActivity));
            mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(
                    SemanticColorUtils.getDefaultBgColor(mActivity));
            mEdgeToEdgeLayoutCoordinator.setStatusBarColor(
                    SemanticColorUtils.getDefaultBgColor(mActivity));
        }
        return mEdgeToEdgeLayoutCoordinator;
    }

    @Nullable EdgeToEdgeLayoutCoordinator getEdgeToEdgeLayoutCoordinatorForTesting() {
        return mEdgeToEdgeLayoutCoordinator;
    }
}
