// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;

import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScopeDependencyProvider;

/** Implementation of {@link PageInsightsSurfaceScopeDependencyProvider}. */
class PageInsightsSurfaceScopeDependencyProviderImpl
        implements PageInsightsSurfaceScopeDependencyProvider {
    private Context mContext;

    PageInsightsSurfaceScopeDependencyProviderImpl(Context context) {
        this.mContext = context;
    }

    @Override
    public Context getActivityContext() {
        return mContext;
    }
}
