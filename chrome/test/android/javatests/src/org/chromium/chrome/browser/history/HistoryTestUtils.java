// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.junit.Assert;

import org.chromium.components.browser_ui.widget.DateDividedAdapter.ItemViewType;

/** Util class for functions and helper classes that share between different test files. */
public class HistoryTestUtils {
    static void checkAdapterContents(
            HistoryAdapter adapter, boolean hasHeader, boolean hasFooter, Object... items) {
        Assert.assertEquals(items.length, adapter.getItemCount());
        Assert.assertEquals(hasHeader, adapter.hasListHeader());
        Assert.assertEquals(hasFooter, adapter.hasListFooter());

        for (int i = 0; i < items.length; i++) {
            if (i == 0 && hasHeader) {
                Assert.assertEquals(ItemViewType.HEADER, adapter.getItemViewType(i));
                continue;
            }

            if (hasFooter && i == items.length - 1) {
                Assert.assertEquals(ItemViewType.FOOTER, adapter.getItemViewType(i));
                continue;
            }

            if (items[i] == null) {
                // TODO(twellington): Check what date header is showing.
                Assert.assertEquals(ItemViewType.DATE, adapter.getItemViewType(i));
            } else {
                Assert.assertEquals(ItemViewType.NORMAL, adapter.getItemViewType(i));
                Assert.assertEquals(items[i], adapter.getItemAt(i).second);
            }
        }
    }
}
