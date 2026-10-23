// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetObserver;
import org.chromium.ui.widget.RectProvider;

/**
 * Provider of the viewport for the fusebox popup window. This implementation treats the entire
 * window as available, ignoring e.g. ime insets which can reduce the available height to a very
 * small quantity using PopupWindow's default viewport rect.
 */
@NullMarked
class ViewportRectProvider extends RectProvider
        implements ComponentCallbacks, OnLayoutChangeListener, WindowInsetObserver {
    private final Activity mActivity;
    private final @Nullable InsetObserver mInsetObserver;
    private final View mView;

    public ViewportRectProvider(
            Activity activity, @Nullable InsetObserver insetObserver, View view) {
        mActivity = activity;
        mInsetObserver = insetObserver;
        if (mInsetObserver != null) {
            mInsetObserver.addObserver(this);
        }
        mView = view;
        mActivity.registerComponentCallbacks(this);
        mView.addOnLayoutChangeListener(this);
        updateRect();
    }

    @Override
    public void onInsetChanged() {
        updateRect();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        updateRect();
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::updateRect);
    }

    private int getTopInset() {
        if (mInsetObserver == null) return 0;

        @Nullable WindowInsetsCompat insets = mInsetObserver.getLastRawWindowInsets();
        if (insets == null) return 0;

        return insets.getInsets(Type.systemBars()).top;
    }

    private void updateRect() {
        Rect bounds =
                WindowMetricsCalculator.getOrCreate()
                        .computeCurrentWindowMetrics(mActivity)
                        .getBounds();
        Rect newRect = new Rect(0, getTopInset(), bounds.width(), bounds.height());
        if (!newRect.equals(mRect)) {
            mRect.set(newRect);
            notifyRectChanged();
        }
    }

    @Override
    public void onLowMemory() {}

    public void destroy() {
        mActivity.unregisterComponentCallbacks(this);
        mView.removeOnLayoutChangeListener(this);
        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
        }
    }
}
