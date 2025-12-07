// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface_provider.hooks.XSurfaceHooks;

/** Holds and provides an instance of {@link ProcessScope}. */
@NullMarked
public final class XSurfaceProcessScopeProvider {
    private static @Nullable ProcessScope sProcessScope;

    public static @Nullable ProcessScope getProcessScope() {
        if (sProcessScope != null) {
            return sProcessScope;
        }
        XSurfaceHooks hooks = ServiceLoaderUtil.maybeCreate(XSurfaceHooks.class);
        if (hooks == null) {
            return null;
        }

        ProcessScopeDependencyProviderFactory dependencyProviderFactory =
                ServiceLoaderUtil.maybeCreate(ProcessScopeDependencyProviderFactory.class);
        assumeNonNull(dependencyProviderFactory);
        sProcessScope = hooks.createProcessScope(dependencyProviderFactory.create());
        return sProcessScope;
    }

    public static void setProcessScopeForTesting(ProcessScope processScope) {
        sProcessScope = processScope;
    }
}
