// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Interface that defines the responsibilities of the layout container for the browser controls.
 *
 * <p>Concrete implementations of this class must extend ViewGroup.
 */
@NullMarked
public interface ControlContainer extends TabStripTransitionDelegate {
    /**
     * Initialize the control container with the specified toolbar.
     *
     * @param toolbarLayoutId The ID of the toolbar layout to use.
     * @param toolbarLayoutHeightResId The ID for the toolbar height resource.
     */
    void initWithToolbar(int toolbarLayoutId, int toolbarLayoutHeightResId);

    /**
     * @return The {@link ViewResourceAdapter} that exposes this {@link View} as a CC resource.
     */
    ViewResourceAdapter getToolbarResourceAdapter();

    /**
     * Get progress bar drawing information.
     * @param drawingInfoOut An instance that the result will be written.
     */
    void getProgressBarDrawingInfo(ClipDrawableProgressBar.DrawingInfo drawingInfoOut);

    int getToolbarBackgroundColor();

    /** Gets the height of the toolbar contained by the control container. */
    int getToolbarHeight();

    /** Gets the height of the toolbar hairline. */
    int getToolbarHairlineHeight();

    /**
     * @param handler The swipe handler to be notified of swipe events on this container.
     */
    void setSwipeHandler(SwipeHandler handler);

    /**
     * @return The {@link View} associated with this container.
     */
    View getView();

    /**
     * Triggered when the current tab or model has changed.
     *
     * @param incognito Whether or not the current tab model is incognito.
     */
    void onTabOrModelChanged(boolean incognito);

    /** Set the compositor background is initialized. */
    void setCompositorBackgroundInitialized();

    /**
     * Returns an instance of the underlying view's layout params that can be mutated; changes will
     * take effect with the next layout pass. A layout pass is requested with each call to this
     * method.
     */
    CoordinatorLayout.LayoutParams mutateLayoutParams();

    /**
     * Returns an instance of the hairline view's layout params that can be mutated; changes will
     * take effect with the next layout pass. A layout pass is requested with each call to this
     * method.
     */
    CoordinatorLayout.LayoutParams mutateHairlineLayoutParams();

    /**
     * Returns an instance of the toolbar view's layout params that can be mutated; changes will
     * take effect with the next layout pass. A layout pass is requested with each call to this
     * method.
     */
    CoordinatorLayout.LayoutParams mutateToolbarLayoutParams();

    /**
     * Toggle display of only the location bar, hiding all other toolbar affordances. This is only
     * valid in cases where there is a location bar view.
     */
    void toggleLocationBarOnlyMode(boolean showOnlyLocationBar);

    /**
     * Add a touch event observer that can observe/intercept touch events that occur within the
     * control container. Observers are processed in order of addition and after any special case
     * handling for, e.g. swipe events or the tab strip.
     */
    void addTouchEventObserver(TouchEventObserver observer);

    /**
     * Remove a touch event observer added via {@link #addTouchEventObserver(TouchEventObserver)}
     */
    void removeTouchEventObserver(TouchEventObserver observer);

    /**
     * Sets the max height for the control container view. The view may be smaller than this and
     * will still wrap to accommodate the height of its children, but only to the specified height.
     */
    void setMaxHeight(int maxHeight);

    /**
     * Destroys the control container, causing it to release any owned native resources and cancel
     * pending tasks.
     */
    void destroy();
}
