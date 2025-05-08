// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Function;

/** The screen that shows a web or native page within a Custom Tab. */
public class CctPageStation extends BasePageStation<CustomTabActivity> {

    /** Builder for CctPageStation. */
    public static class Builder
            extends BasePageStation.Builder<CustomTabActivity, CctPageStation, Builder> {
        protected boolean mIsNativePage;

        public Builder(Function<Builder, CctPageStation> factoryMethod) {
            super(factoryMethod);
        }

        public Builder withIsNativePage(boolean isNativePage) {
            mIsNativePage = isNativePage;
            return this;
        }
    }

    protected boolean mIsNativePage;
    public Element<WebContents> webContentsElement;

    public static Builder newBuilder() {
        return new Builder(CctPageStation::new);
    }

    protected CctPageStation(Builder builder) {
        super(CustomTabActivity.class, builder);

        // isNativePage is optional and defaults to false
        mIsNativePage = builder.mIsNativePage;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        if (!mIsNativePage) {
            webContentsElement =
                    elements.declareEnterConditionAsElement(
                            new WebContentsPresentCondition(loadedTabElement));
            elements.declareEnterCondition(new FrameInfoUpdatedCondition(webContentsElement));
        }
    }
}
