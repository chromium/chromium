// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import java.util.List;

/** A source of Chips meant to be visually represented by a {@link ChipsCoordinator}. */
public interface ChipsProvider {
    /** Interface to be called a Chip's state changes. */
    interface Observer {
        /** Called whenever the list of Chips or selection changes. */
        void onChipsChanged();
    }

    /** Adds an {@link Observer} to be notified of Chip state changes. */
    void addObserver(Observer observer);

    /** Removes an {@link Observer} to be notified of Chip state changes. */
    void removeObserver(Observer observer);

    /** @return A list of {@link Chip} objects that are currently visible. */
    List<Chip> getChips();

    /** @return The spacing between the chips in pixels. */
    int getChipSpacingPx();

    /** @return The pixels in front of the first chip or the pixels after the last chip. */
    int getSidePaddingPx();
}
