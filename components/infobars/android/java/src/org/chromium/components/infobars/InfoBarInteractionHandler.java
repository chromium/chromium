// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import org.chromium.build.annotations.NullMarked;

/** Functions needed to display an InfoBar UI. */
@NullMarked
public interface InfoBarInteractionHandler {
    /** Handles click on the infobar. It is invoked before one of the following functions. */
    void onClick();

    /** Takes some action related to the link being clicked. */
    void onLinkClicked();

    /** Takes some action related to the close button being clicked. */
    void onCloseButtonClicked();

    /**
     * Performs some action related to either the primary or secondary button being pressed.
     *
     * @param isPrimaryButton True if the primary button was clicked, false otherwise.
     */
    void onButtonClicked(boolean isPrimaryButton);
}
