// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.junit.Assert;

import org.chromium.components.browser_ui.widget.DateDividedAdapter.ItemViewType;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.TimedItem;

/** Util class for functions and helper classes that share between different test files. */
public class HistoryTestUtils {
    static void checkAdapterContents(
            HistoryAdapter adapter,
            boolean hasStandardHeader,
            boolean hasFooter,
            TimedItem... items) {
        checkAdapterContents(
                adapter, hasStandardHeader, /* hasPersistentHeader= */ false, hasFooter, items);
    }

    static void checkAdapterContents(
            HistoryAdapter adapter,
            boolean hasStandardHeader,
            boolean hasPersistentHeader,
            boolean hasFooter,
            TimedItem... items) {
        Assert.assertEquals(items.length, adapter.getItemCount());
        Assert.assertEquals(hasStandardHeader || hasPersistentHeader, adapter.hasListHeader());
        Assert.assertEquals(hasFooter, adapter.hasListFooter());
        int persistentHeaderPosition = hasPersistentHeader ? (hasStandardHeader ? 1 : 0) : -1;

        for (int i = 0; i < items.length; i++) {
            if (i == 0 && hasStandardHeader) {
                Assert.assertEquals(ItemViewType.STANDARD_HEADER, adapter.getItemViewType(i));
                continue;
            }

            if (i == persistentHeaderPosition) {
                Assert.assertEquals(ItemViewType.PERSISTENT_HEADER, adapter.getItemViewType(i));
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
