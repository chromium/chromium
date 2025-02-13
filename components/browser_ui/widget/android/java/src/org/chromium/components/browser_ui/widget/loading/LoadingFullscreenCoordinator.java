// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.loading;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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
     */
    public void startLoading(Runnable onFinishCallback) {
        @ColorInt int backgroundColor = SemanticColorUtils.getDefaultBgColor(mActivity);
        Runnable onClickRunnable =
                () -> {
                    onFinishCallback.run();
                };

        mPropertyModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, mContainer)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.BACKGROUND_COLOR, backgroundColor)
                        .with(ScrimProperties.CLICK_DELEGATE, onClickRunnable)
                        .build();

        mScrimManager.showScrim(mPropertyModel);
    }

    /** Close the loading screen that's showing. */
    public void closeLoadingScreen() {
        if (mPropertyModel != null) {
            mScrimManager.hideScrim(mPropertyModel, /* animate= */ true);
        }
    }

    /** Performs tear down. */
    public void destroy() {
        closeLoadingScreen();
    }
}
