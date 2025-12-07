// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Element;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.content_public.browser.WebContents;

/** The screen that shows a web or native page within a Custom Tab. */
public class CctPageStation extends BasePageStation<CustomTabActivity> {
    public Element<WebContents> webContentsElement;

    public static Builder<CctPageStation> newBuilder() {
        return new Builder<>(CctPageStation::new);
    }

    protected CctPageStation(Config config) {
        super(CustomTabActivity.class, config);

        // TODO(crbug.com/413111192): Support native CCT pages, e.g. PDF pages.
        webContentsElement =
                declareEnterConditionAsElement(new WebContentsPresentCondition(loadedTabElement));
        declareEnterCondition(new FrameInfoUpdatedCondition(webContentsElement));
    }
}
