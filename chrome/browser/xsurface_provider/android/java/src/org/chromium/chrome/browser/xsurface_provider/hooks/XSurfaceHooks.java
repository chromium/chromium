// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider.hooks;

import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;

/** Provides access to internal XSurface implementations, if they are available. */
public interface XSurfaceHooks {
    /** Whether the internal implementations of XSurface interfaces are available.*/
    default boolean isEnabled() {
        return false;
    }

    /** Creates and returns a {@link ProcessScope}. */
    ProcessScope createProcessScope(ProcessScopeDependencyProvider dependencies);
}
