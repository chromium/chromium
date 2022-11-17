// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.lifetime.Destroyable;

/**
 * An interface for retrieving and monitoring browser controls state. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantBrowserControls extends Destroyable {
    /**
     * Observer for different browser control events.
     */
    public interface Observer {
        void onControlsOffsetChanged();
        void onBottomControlsHeightChanged();
    }

    void setObserver(Observer browserControlsObserver);

    int getBottomControlsHeight();
    int getBottomControlOffset();
    int getContentOffset();
    float getTopVisibleContentOffset();
}
