// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.webapps;

import android.content.Intent;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.webapps.WebappIntentDataProviderFactory;

/** Helper class for webapp tests. */
public class WebappTestHelper {
    /**
     * Returns simplest intent which builds valid WebappInfo via
     * {@link WebappIntentDataProviderFactory#create()}.
     */
    public static Intent createMinimalWebappIntent(String id, String url) {
        Intent intent = new Intent();
        intent.putExtra(WebappConstants.EXTRA_ID, id);
        intent.putExtra(WebappConstants.EXTRA_URL, url);
        return intent;
    }

    public static BrowserServicesIntentDataProvider createIntentDataProvider(
            String id, String url) {
        return WebappIntentDataProviderFactory.create(createMinimalWebappIntent(id, url));
    }
}
