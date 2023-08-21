// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;

/**
 * Provides instance of ProcessScopeDependencyProvider needed in response to Native calls.
 *
 * Note that ProcessScopeDependencyProvider is sometimes needed in response to a Native call,
 * we use this through reflection rather than simply injecting an instance.
 */
public interface ProcessScopeDependencyProviderFactory {
    /** Constructs and returns a ProcessScopeDependencyProvider instance. */
    ProcessScopeDependencyProvider create();
}
