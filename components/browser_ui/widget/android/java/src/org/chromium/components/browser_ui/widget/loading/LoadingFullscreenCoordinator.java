// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.loading;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** The class for managing a loading screen on top of a Chrome activity. */
@NullMarked
public class LoadingFullscreenCoordinator {
    private final ScrimManager mScrimManager;
    private final Activity mActivity;
    private final ViewGroup mContainer;
    private @Nullable PropertyModel mPropertyModel;

    /**
     * @param activity The {@link Activity} on which the loading shows.
     * @param scrimManager The {@link ScrimManage} for creating scrim.
     * @param container The view that contains the loading UI.
     */
    public LoadingFullscreenCoordinator(
            Activity activity, ScrimManager scrimManager, ViewGroup container) {
        mActivity = activity;
        mScrimManager = scrimManager;
        mContainer = container;
    }

    /**
     * Start showing the loading screen.
     *
     * @param onFinishCallback The callback to call when the loading screen exits.
     * @param animate If the screen should animate.
     */
    public void startLoading(Runnable onFinishCallback, boolean animate) {
        @ColorInt int backgroundColor = SemanticColorUtils.getDefaultBgColor(mActivity);
        View.OnClickListener cancelButtonClickListener =
                (view) -> {
                    onFinishCallback.run();
                };
        mContainer
                .findViewById(R.id.loading_cancel_button)
                .setOnClickListener(cancelButtonClickListener);

        // Do not stack two loading screens.
        if (mPropertyModel != null) {
            closeLoadingScreen();
        }

        mPropertyModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, mContainer)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.BACKGROUND_COLOR, backgroundColor)
                        .build();

        mScrimManager.showScrim(mPropertyModel, animate);
        mContainer.setVisibility(View.VISIBLE);
    }

    /** Close the loading screen that's showing. */
    public void closeLoadingScreen() {
        if (mPropertyModel != null) {
            mScrimManager.hideScrim(mPropertyModel, /* animate= */ true);
            mContainer.setVisibility(View.GONE);
        }
    }

    /** Performs tear down. */
    public void destroy() {
        closeLoadingScreen();
    }
}
