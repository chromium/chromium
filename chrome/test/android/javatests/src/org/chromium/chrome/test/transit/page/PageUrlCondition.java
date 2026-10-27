// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.base.test.transit.Condition.whether;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Supplier;

/**
 * Condition to check the page url matches a {@link Matcher}, e.g. {@code is(url)} or {@code
 * containsString(substring)}.
 */
public class PageUrlCondition extends Condition {
    private final Matcher<String> mUrlMatcher;
    private final Supplier<Tab> mLoadedTabSupplier;

    public PageUrlCondition(Matcher<String> urlMatcher, Supplier<Tab> loadedTabSupplier) {
        super(/* isRunOnUiThread= */ true);
        mUrlMatcher = urlMatcher;
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        String url = mLoadedTabSupplier.get().getUrl().getSpec();
        return whether(mUrlMatcher.matches(url), "Page URL: \"%s\"", url);
    }

    @Override
    public String buildDescription() {
        return "Page URL: " + StringDescription.toString(mUrlMatcher);
    }
}
