// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;

/**
 * Callback interface to allow native pages to intercept and veto/delay tab closure or back
 * navigation (similar to window.onbeforeunload in web pages).
 */
@NullMarked
public interface BeforeUnloadCallback extends UserData {
    /**
     * Called before unloading the page.
     *
     * @param onProceed Runnable to call if the user decides to proceed with the unload.
     * @param onCancel Runnable to call if the user decides to cancel the unload.
     * @return true if the interception is handled (e.g. dialog shown), false otherwise.
     */
    boolean handleBeforeUnload(Runnable onProceed, Runnable onCancel);
}
