// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowHeuristicResult;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderStateProvider;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.insets.CaptionBarInsetsRectProvider;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

/**
 * Basic implementation of {@link AppHeaderStateProvider} that detects and stores the current app
 * header state.
 */
@RequiresApi(VERSION_CODES.R)
@NullMarked
public class BasicAppHeaderStateProvider implements AppHeaderStateProvider {

    private final ImmutableWeakReference<Activity> mActivity;
    private final CaptionBarInsetsRectProvider mCaptionBarRectProvider;
    private @Nullable AppHeaderState mAppHeaderState;

    /**
     * Instantiate the coordinator to handle drawing the tab strip into the captionBar area.
     *
     * @param activity The activity associated with the window containing the app header.
     * @param insetObserver {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     */
    public BasicAppHeaderStateProvider(Activity activity, InsetObserver insetObserver) {
        mActivity = new ImmutableWeakReference<>(activity);

        // Initialize mInsetsRectProvider and setup observers.
        mCaptionBarRectProvider =
                new CaptionBarInsetsRectProvider(
                        insetObserver,
                        insetObserver.getLastRawWindowInsets(),
                        InsetConsumerSource.APP_HEADER_COORDINATOR_CAPTION);

        // Set the insets consumers and trigger insets application for potential consumption after
        // the rect provider is ready, to populate initial values.
        mCaptionBarRectProvider.setConsumer(this::updateAppHeaderState);
        insetObserver.retriggerOnApplyWindowInsets();
    }

    @VisibleForTesting
    BasicAppHeaderStateProvider(
            Activity activity, CaptionBarInsetsRectProvider insetsRectProvider) {
        mActivity = new ImmutableWeakReference<>(activity);
        mCaptionBarRectProvider = insetsRectProvider;
        mCaptionBarRectProvider.setConsumer(this::updateAppHeaderState);
    }

    @Override
    public @Nullable AppHeaderState getAppHeaderState() {
        return mAppHeaderState;
    }

    private boolean updateAppHeaderState(Rect widestUnoccludedRect) {
        if (mActivity.get() == null) return false;

        int heuristicResult =
                AppHeaderUtils.checkIsInDesktopWindow(mCaptionBarRectProvider, mActivity.get());
        var isInDesktopWindow = heuristicResult == DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW;

        mAppHeaderState =
                new AppHeaderState(
                        mCaptionBarRectProvider.getWindowRect(),
                        widestUnoccludedRect,
                        isInDesktopWindow);

        // Always return false so it does not consume the insets.
        return false;
    }
}
