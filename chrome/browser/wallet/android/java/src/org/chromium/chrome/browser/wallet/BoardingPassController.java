// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.wallet;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

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
                if (shouldDetect(url.getSpec())) {
                    Log.d(TAG, "Detect boarding pass on url: %s", url.getSpec());
                }
            }
        };
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }

    private static boolean shouldDetect(String url) {
        // TODO(crbug/1502330): Move shouldDetect logic to native code.
        List<String> allowedUrls = getAllowedUrls();
        Log.d(TAG, "allowed urls: %s, size: %d", allowedUrls, allowedUrls.size());
        return allowedUrls.stream().anyMatch(urlPrefix -> url.startsWith(urlPrefix));
    }

    private static List<String> getAllowedUrls() {
        String paramVal =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.BOARDING_PASS_DETECTOR, "boarding_pass_detector_urls");
        String[] urls = paramVal.trim().split(",");
        List<String> allowedUrls = new ArrayList<>();
        for (String url : urls) {
            String trimedUrl = url.trim();
            if (!trimedUrl.isEmpty()) {
                allowedUrls.add(trimedUrl);
            }
        }
        return allowedUrls;
    }
}
