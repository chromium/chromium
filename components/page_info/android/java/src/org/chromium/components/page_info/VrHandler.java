// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import androidx.annotation.IntDef;

/**
 * A delegate class to handle unsupported UI in VR mode.
 * TODO: Move to components/browser_ui if more components need this.
 */
public interface VrHandler {
    @IntDef({UiType.CERTIFICATE_INFO, UiType.CONNECTION_SECURITY_INFO})
    public @interface UiType {
        int CERTIFICATE_INFO = 0;
        int CONNECTION_SECURITY_INFO = 1;
    };

    /**
     * Returns true if we're currently in VR mode, false otherwise.
     */
    boolean isInVr();

    /**
     * Exits VR mode and runs the runnable passed in. Must only be called when
     * we're currently in VR mode.
     * @param r The runnable to be run after exiting VR mode.
     * @param uiType Used for logging the UI type that is unsupported in VR and
     * caused us to exit VR mode.
     */
    void exitVrAndRun(Runnable r, @UiType int uiType);
}
