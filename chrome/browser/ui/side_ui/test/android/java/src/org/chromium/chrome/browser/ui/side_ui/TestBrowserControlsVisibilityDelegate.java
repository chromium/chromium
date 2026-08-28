// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;

/**
 * Test implementation of {@link BrowserStateBrowserControlsVisibilityDelegate} for tracking token
 * acquisitions and releases.
 */
@NullMarked
public class TestBrowserControlsVisibilityDelegate
        extends BrowserStateBrowserControlsVisibilityDelegate {
    public int mShowCount;
    public int mReleaseCount;

    public TestBrowserControlsVisibilityDelegate() {
        super(ObservableSuppliers.createNonNull(false));
    }

    @Override
    public int showControlsPersistent() {
        mShowCount++;
        return super.showControlsPersistent();
    }

    @Override
    public void releasePersistentShowingToken(int token) {
        mReleaseCount++;
        super.releasePersistentShowingToken(token);
    }
}
