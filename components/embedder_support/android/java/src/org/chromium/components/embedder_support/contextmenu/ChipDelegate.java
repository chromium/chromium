// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import org.chromium.base.Callback;

/** Interface to handle chip data and actions. */
public interface ChipDelegate {
    /**
     * Determines whether the chip delegate is able to support a chip in the chosen context.
     *
     * @return Whether the chip can be supported and rendered.
     */
    boolean isChipSupported();

    /**
     * Retrieves the data for displaying a chip below the context menu.
     *
     * @param callback The callback will always be called with the retrieved ChipRenderParams. The
     *     ChipRenderParams will be null in the event there is no chip to show.
     */
    void getChipRenderParams(Callback<ChipRenderParams> callback);

    /** Called when the context menu is closed. */
    void onMenuClosed();

    /**
     * Check whether the ChipRenderParams object is valid. Called before displaying the chip.
     *
     * @param chipRenderParams A wrapper object contains the params used when rendering the chip.
     * @return True if the params are valid.
     */
    boolean isValidChipRenderParams(ChipRenderParams chipRenderParams);
}
