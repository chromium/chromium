// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;

/**
 * A {@link LayoutStateObserver} that filters events based on a specified layout ID. This utility is
 * helpful if a feature wishes to exclusively listen to events on a specific layout without the need
 * for many conditionals.
 */
public final class FilterLayoutStateObserver implements LayoutStateObserver {
    /** The observer that will receive the filtered events. */
    private final LayoutStateObserver mObserver;

    /** The type to filter on. */
    @LayoutType private final int mType;

    /**
     * Create a filtered observer that only filters on a single layout type.
     * @param layoutType The layout type to filter on.
     * @param observer The observer to send filtered events to.
     */
    public FilterLayoutStateObserver(@LayoutType int layoutType, LayoutStateObserver observer) {
        mObserver = observer;
        mType = layoutType;
    }

    @Override
    public void onStartedShowing(int layoutType) {
        if (layoutType != mType) return;
        mObserver.onStartedShowing(layoutType);
    }

    @Override
    public void onFinishedShowing(int layoutType) {
        if (layoutType != mType) return;
        mObserver.onFinishedShowing(layoutType);
    }

    @Override
    public void onStartedHiding(int layoutType) {
        if (layoutType != mType) return;
        mObserver.onStartedHiding(layoutType);
    }

    @Override
    public void onFinishedHiding(int layoutType) {
        if (layoutType != mType) return;
        mObserver.onFinishedHiding(layoutType);
    }
}
