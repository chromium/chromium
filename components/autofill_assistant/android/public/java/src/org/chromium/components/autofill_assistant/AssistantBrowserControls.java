// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.lifetime.Destroyable;

/**
 * An interface for retrieving and monitoring browser controls state.. Implementations might differ
 * depending on where Autofill Assistant is running (e.g. WebLayer, Chrome).
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
