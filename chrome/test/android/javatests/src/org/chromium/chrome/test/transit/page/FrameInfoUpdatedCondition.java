// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;

/** Fulfilled when FrameInfo of the WebContents provided has been updated. */
public class FrameInfoUpdatedCondition extends UiThreadCondition {
    Supplier<WebContents> mWebContentsSupplier;

    public FrameInfoUpdatedCondition(Supplier<WebContents> webContentsSupplier) {
        mWebContentsSupplier = dependOnSupplier(webContentsSupplier, "WebContents");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        Coordinates coordinates = Coordinates.createFor(mWebContentsSupplier.get());
        return whether(
                coordinates.frameInfoUpdated(),
                "frameInfoUpdated %b, pageScaleFactor: %.2f",
                coordinates.frameInfoUpdated(),
                coordinates.getPageScaleFactor());
    }

    @Override
    public String buildDescription() {
        return "WebContents frame info updated";
    }
}
