// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.wallet;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/** Controls the whole flow of boarding pass detection. */
public class BoardingPassController {
    private static final String TAG = "BoardingPassCtrl";

    private final EmptyTabObserver mTabObserver;
    private final CurrentTabObserver mCurrentTabObserver;

    public BoardingPassController(ObservableSupplier<Tab> tabSupplier) {
        mTabObserver = createTabObserver();
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, mTabObserver);
    }

    private EmptyTabObserver createTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                Log.d(TAG, "page loaded for: " + url.getSpec());
                // TODO(crbug/1502330): Trigger detection if url is in allowlist.
            }
        };
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }
}
