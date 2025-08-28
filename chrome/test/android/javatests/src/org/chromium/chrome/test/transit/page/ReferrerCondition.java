// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.function.Supplier;

/** Condition checking that the page referrer has the expected value. */
public class ReferrerCondition extends Condition {
    private final Supplier<Tab> mTabSupplier;
    private final String mExpectedReferrer;
    private static final String GET_REFERRER_JS = "(function() { return document.referrer; })();";

    public ReferrerCondition(Supplier<Tab> tabSupplier, String expectedReferrer) {
        super(/* isRunOnUiThread= */ false);
        mTabSupplier = dependOnSupplier(tabSupplier, "Tab");
        // Add quotes to match returned value from JS.
        mExpectedReferrer = "\"" + expectedReferrer + "\"";
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTabSupplier.get().getWebContents(), GET_REFERRER_JS);
        if (jsonText.equalsIgnoreCase("null")) {
            jsonText = "";
        }
        String referrer = jsonText;
        return whetherEquals(mExpectedReferrer, referrer);
    }

    @Override
    public String buildDescription() {
        return "Referrer is " + mExpectedReferrer;
    }
}
