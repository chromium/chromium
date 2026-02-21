// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.TypedArray;
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

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.edge_to_edge.WindowSystemBarColorHelper;
import org.chromium.ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;
import org.chromium.ui.util.AttrUtils;

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
    @Nullable private WindowInsetsConsumer mWindowInsetsConsumer;
    private final boolean mShouldPadForWindowInsets;
    private final WindowSystemBarColorHelper mWindowColorHelper;

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

        TypedArray a = getContext().obtainStyledAttributes(themeResId, R.styleable.ChromeDialog);
        mIsFullScreen = a.getBoolean(R.styleable.ChromeDialog_isDialogFullscreen, false);
        a.recycle();

        mShouldPadForWindowInsets = shouldPadForWindowInsets;
        if (getWindow() != null) {
            mInsetObserver =
                    new InsetObserver(
                            new ImmutableWeakReference<>(getWindow().getDecorView().getRootView()),
                            new ImmutableWeakReference<>(mActivity),
                            // Keyboard overlay mode is enabled by default and is currently only
                            // relevant to the DeferredImeWindowInsetApplicationCallback.
                            /* enableKeyboardOverlayMode= */ true,
                            /* enableExtraEdgeToEdgeLogging= */ false);
        }
        // Currently, only the EdgeToEdgeLayoutCoordinator is listening to this InsetObserver,
        // and that class can handle cases with a null Window / null InsetObserver. Before
        // adding any new Insets observers / consumers, ensure that the window is not null / the
        // InsetObserver is reliably being created.
        // TODO(crbug.com/402995103): Clean up getWindow() null check assert.
        assert getWindow() != null : "Checking if there are cases when getWindow() is null";
        mWindowColorHelper = new WindowSystemBarColorHelper(getWindow());
    }

    /**
     * Registers a window insets consumer with a specified priority.
     *
     * <p>Consumers are notified in the order of their pre-defined priority value as defined in
     * {@link InsetConsumerSource}. A lower-indexed source allows this consumer to process insets
     * <b>before</b> components with higher-indexed sources.
     *
     * @param insetConsumer The consumer to receive window insets.
     * @param source The priority source from {@link InsetConsumerSource}.
     */
    public void addInsetsConsumer(
            WindowInsetsConsumer insetConsumer, @InsetConsumerSource int source) {
        if (mInsetObserver == null) return;

        mWindowInsetsConsumer = insetConsumer;
        mInsetObserver.addInsetsConsumer(insetConsumer, source);
    }

    @Override
    public void setContentView(@LayoutRes int layoutResID) {
        if (DeviceInfo.isAutomotive() && mIsFullScreen) {
            super.setContentView(
                    AutomotiveUtils.getAutomotiveLayoutWithBackButtonToolbar(mActivity));
            setAutomotiveToolbarBackButtonAction();
            ViewStub stub = findViewById(R.id.original_layout);
            stub.setLayoutResource(layoutResID);
            stub.inflate();
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
        if (DeviceInfo.isAutomotive() && mIsFullScreen) {
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
        if (DeviceInfo.isAutomotive() && mIsFullScreen) {
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
        if (DeviceInfo.isAutomotive()
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

    /**
     * Set the navigation bar color. This is useful when the view attaching to the nav bar is
     * different than the dialog's background.
     *
     * @param color Nav bar color for the current dialog.
     */
    public void setNavBarColor(int color) {
        if (mEdgeToEdgeLayoutCoordinator != null) {
            mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(color);
        } else {
            mWindowColorHelper.setNavigationBarColor(color);
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
                    AttrUtils.resolveColor(
                            getContext().getTheme(), android.R.attr.navigationBarColor));
            mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(
                    AttrUtils.resolveColor(
                            getContext().getTheme(), android.R.attr.navigationBarDividerColor));
            mEdgeToEdgeLayoutCoordinator.setStatusBarColor(
                    AttrUtils.resolveColor(getContext().getTheme(), android.R.attr.statusBarColor));
        }
        return mEdgeToEdgeLayoutCoordinator;
    }

    @Nullable EdgeToEdgeLayoutCoordinator getEdgeToEdgeLayoutCoordinatorForTesting() {
        return mEdgeToEdgeLayoutCoordinator;
    }

    public void destroy() {
        if (isShowing()) {
            dismiss();
        }
        if (mInsetObserver != null && mWindowInsetsConsumer != null) {
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
            mWindowInsetsConsumer = null;
        }
    }
}
