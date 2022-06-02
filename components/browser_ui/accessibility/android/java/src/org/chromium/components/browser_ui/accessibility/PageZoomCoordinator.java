// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the page zoom feature. Created by the |RootUiCoordinator| and acts as the
 * public API for the component. Classes outside the component wishing to interact with page
 * zoom should be calling methods in this class only.
 */
public class PageZoomCoordinator {
    private final Delegate mDelegate;
    private final PropertyModel mModel;
    private final PageZoomMediator mMediator;
    private View mView;

    private static Boolean sShouldShowMenuItemForTesting;

    /**
     * Delegate interface for any class that wants a |PageZoomCoordinator| and to display the view.
     */
    public interface Delegate {
        View getZoomControlView();
    }

    public PageZoomCoordinator(Delegate delegate) {
        mDelegate = delegate;
        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS).build();
        mMediator = new PageZoomMediator(mModel);
    }

    /**
     * Whether or not the AppMenu should show the 'Zoom' menu item.
     * @return boolean
     */
    public static boolean shouldShowMenuItem() {
        if (sShouldShowMenuItemForTesting != null) return sShouldShowMenuItemForTesting;
        return PageZoomMediator.shouldShowMenuItem();
    }

    /**
     * Show the zoom feature UI to the user.
     * @param webContents   WebContents that this zoom UI will control.
     */
    public void show(WebContents webContents) {
        // If the view has not been created, lazily inflate from the view stub.
        if (mView == null) {
            mView = mDelegate.getZoomControlView();
            PropertyModelChangeProcessor.create(mModel, mView, PageZoomViewBinder::bind);
        } else {
            mView.setVisibility(View.VISIBLE);
        }
        mMediator.setWebContents(webContents);

        // TODO(mschillaci): Remove this when proper dismiss conditions are added.
        mView.postDelayed(this::hide, 50000);
    }

    /**
     * Hide the zoom feature UI from the user.
     */
    public void hide() {
        // TODO(mschillaci): Add a FrameLayout wrapper so the view can be removed.
        mView.setVisibility(View.GONE);
    }

    /**
     * Clean-up views and children during destruction.
     */
    public void destroy() {}

    /**
     * Used for testing only, allows a mocked value for the {@link shouldShowMenuItem} method.
     * @param isEnabled     Should show the menu item or not.
     */
    @VisibleForTesting
    public static void setShouldShowMenuItemForTesting(@Nullable Boolean isEnabled) {
        sShouldShowMenuItemForTesting = isEnabled;
    }
}
