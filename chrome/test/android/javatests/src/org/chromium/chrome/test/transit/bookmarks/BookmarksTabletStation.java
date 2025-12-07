// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.bookmarks;

import static org.chromium.base.test.transit.Condition.whetherEquals;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;

import org.chromium.base.test.transit.Element;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.NativePageCondition;

/** The Bookmarks page in tablets and desktop. */
public class BookmarksTabletStation extends CtaPageStation {
    public Element<BookmarkPage> nativePageElement;

    public static Builder<BookmarksTabletStation> newBuilder() {
        return new Builder<>(BookmarksTabletStation::new);
    }

    public BookmarksTabletStation(Config config) {
        super(config);

        nativePageElement =
                declareEnterConditionAsElement(
                        new NativePageCondition<>(BookmarkPage.class, loadedTabElement));
        declareEnterCondition(
                uiThreadCondition(
                        "NativePage has title \"Bookmarks\"",
                        nativePageElement,
                        nativePage -> whetherEquals("Bookmarks", nativePage.getTitle())));
    }
}
