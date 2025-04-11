// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Root component to interact with web app header. This coordinator lazily initializes web app
 * header when {@link DesktopWindowStateManager} indicates that the view hierarchy is in the desktop
 * window.
 */
@NullMarked
public class WebAppHeaderLayoutCoordinator implements DesktopWindowStateManager.AppHeaderObserver {

    private @Nullable WebAppHeaderLayoutMediator mMediator;
    private @Nullable ViewGroup mView;
    private final ViewStub mViewStub;
    private final DesktopWindowStateManager mDesktopWindowStateManager;

    /**
     * Creates an instance of {@link WebAppHeaderLayoutCoordinator}.
     *
     * @param viewStub a stub in which web app header will be inflated into.
     * @param desktopWindowStateManager a class that notifies about desktop windowing state changes.
     */
    public WebAppHeaderLayoutCoordinator(
            ViewStub viewStub, DesktopWindowStateManager desktopWindowStateManager) {
        mViewStub = viewStub;
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);
        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        if (!newState.isInDesktopWindow()) return;
        ensureInitialized();
    }

    private void ensureInitialized() {
        if (mView != null) return;

        mView = (ViewGroup) mViewStub.inflate();
        final var model = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        final int headerMinHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.web_app_header_min_height);
        mMediator =
                new WebAppHeaderLayoutMediator(model, mDesktopWindowStateManager, headerMinHeight);

        PropertyModelChangeProcessor.create(model, mView, WebAppHeaderLayoutViewBinder::bind);

        // TODO(vkorotkevich): create reload button coordinator here.
    }

    /**
     * Cleans up resources and subscriptions. This class should not be used after this method is
     * called.
     */
    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);

        if (mMediator == null) return;
        mMediator.destroy();
    }
}
