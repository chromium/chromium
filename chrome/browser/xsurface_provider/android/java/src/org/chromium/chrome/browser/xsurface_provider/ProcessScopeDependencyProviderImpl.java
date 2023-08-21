// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import org.chromium.chrome.browser.feed.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;

/** Implementation of {@link ProcessScopeDependencyProvider}. */
// TODO(b/286003870): Stop extending FeedProcessScopeDependencyProvider, and
// remove all dependencies on Feed library.
public class ProcessScopeDependencyProviderImpl extends FeedProcessScopeDependencyProvider {
    public ProcessScopeDependencyProviderImpl(
            String apiKey, PrivacyPreferencesManager privacyPreferencesManager) {
        super(apiKey, privacyPreferencesManager);
    }
}
