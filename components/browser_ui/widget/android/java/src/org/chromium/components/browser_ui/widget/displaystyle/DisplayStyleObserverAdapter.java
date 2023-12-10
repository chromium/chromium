// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.displaystyle;

import android.view.View;

/**
 * Implementation of {@link DisplayStyleObserver} designed to play nicely with
 * {@link androidx.recyclerview.widget.RecyclerView}. It will not notify of changes when the
 * associated view is not attached to the window.
 */
public class DisplayStyleObserverAdapter
        implements DisplayStyleObserver, View.OnAttachStateChangeListener {
    private final DisplayStyleObserver mObserver;

    /** Current display style, gets updated as the UiConfig detects changes and notifies us. */
    private UiConfig.DisplayStyle mCurrentDisplayStyle;

    private boolean mIsViewAttached;

    private final UiConfig mUiConfig;

    /**
     * @param view the view whose lifecycle is tracked to determine when to not fire the
     *             observer.
     * @param config the {@link UiConfig} object to subscribe to.
     * @param observer the observer to adapt. It's {#onDisplayStyleChanged} will be called when
     *                 the configuration changes, provided that {@code view} is attached to the
     *                 window.
     */
    public DisplayStyleObserverAdapter(View view, UiConfig config, DisplayStyleObserver observer) {
        mUiConfig = config;
        mObserver = observer;
        mIsViewAttached = view.isAttachedToWindow();

        view.addOnAttachStateChangeListener(this);
    }

    /** Attaches to the {@link #mUiConfig}. */
    public void attach() {
        // This call will also assign the initial value to |mCurrentDisplayStyle|.
        mUiConfig.addObserver(this);
    }

    /** Detaches from the {@link #mUiConfig}. */
    public void detach() {
        mUiConfig.removeObserver(this);
    }

    @Override
    public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        assert newDisplayStyle != null;
        mCurrentDisplayStyle = newDisplayStyle;

        if (!mIsViewAttached) return;
        mObserver.onDisplayStyleChanged(mCurrentDisplayStyle);
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        mIsViewAttached = true;
        onDisplayStyleChanged(mCurrentDisplayStyle);
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        mIsViewAttached = false;
    }
}
