// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

/**
 * Observer interface for any object that needs to process touch events. This is useful when
 * ordinary Android view events processing hierarchy cannot be easily applied.
 */
public interface TouchEventProvider {
    /** @param obs {@link TouchEventObserver} object to process. */
    void addTouchEventObserver(TouchEventObserver obs);

    /**
     * Removes the registered {@link TouchEventObserver}.
     *
     * @param obs {@link TouchEventObserver} object to process.
     */
    void removeTouchEventObserver(TouchEventObserver obs);
}
