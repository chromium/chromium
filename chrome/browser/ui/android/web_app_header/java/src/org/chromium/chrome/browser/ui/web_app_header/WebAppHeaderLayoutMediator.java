// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator that listens for window state changes and updates custom webapp header view that should
 * take the place of caption bar/title bar/app header at the top of the window.
 */
@NullMarked
class WebAppHeaderLayoutMediator implements DesktopWindowStateManager.AppHeaderObserver {
    @Nullable static Integer sMinHeaderHeightForTesting;

    private final Rect mCachedPaddings;
    private final PropertyModel mModel;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final int mWebAppMinHeaderHeight;

    /**
     * Constructs the instance of {@link WebAppHeaderLayoutMediator}.
     *
     * @param model model that encapsulates UI state for the view
     * @param desktopWindowStateManager desktop window state manager that provides updates on window
     *     state
     * @param webAppHeaderMinHeightFromResources minimal height from resources that web app header
     *     must take
     */
    public WebAppHeaderLayoutMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            int webAppHeaderMinHeightFromResources) {
        mWebAppMinHeaderHeight = webAppHeaderMinHeightFromResources;
        mDesktopWindowStateManager = desktopWindowStateManager;

        final var modelPaddings = model.get(WebAppHeaderLayoutProperties.PADDINGS);
        mCachedPaddings = modelPaddings != null ? modelPaddings : new Rect(0, 0, 0, 0);

        mModel = model;

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
        mDesktopWindowStateManager.addObserver(this);
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        updatePaddings(newState);
        mModel.set(
                WebAppHeaderLayoutProperties.MIN_HEIGHT,
                Math.max(newState.getAppHeaderHeight(), getDefaultMinHeight()));
        mModel.set(WebAppHeaderLayoutProperties.IS_VISIBLE, newState.isInDesktopWindow());
    }

    private void updatePaddings(AppHeaderState newState) {
        // Matching behavior to BrApp: add top padding when caption bar insets are greater than our
        // min height expectations. Rationale - some vendors provide caption bar insets as
        // caption bar + status bar insets, to layout properly we need to deduct status bar insets.
        // This assumption is optimistic - we assume that caption bar matches our min height.
        final int topPadding = Math.max(0, newState.getAppHeaderHeight() - getDefaultMinHeight());

        mCachedPaddings.set(
                newState.getLeftPadding(),
                topPadding,
                newState.getRightPadding(),
                mCachedPaddings.bottom);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, mCachedPaddings);
    }

    private int getDefaultMinHeight() {
        if (sMinHeaderHeightForTesting != null) return sMinHeaderHeightForTesting;
        return mWebAppMinHeaderHeight;
    }

    static void setMinHeightForTesting(final int height) {
        sMinHeaderHeightForTesting = height;
        ResettersForTesting.register(() -> sMinHeaderHeightForTesting = null);
    }

    /** Destroys the mediator, existing instance is not usable after this method is called */
    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
    }
}
