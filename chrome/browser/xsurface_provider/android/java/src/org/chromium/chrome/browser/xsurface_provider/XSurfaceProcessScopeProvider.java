// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface_provider.hooks.XSurfaceHooks;
import org.chromium.chrome.browser.xsurface_provider.hooks.XSurfaceHooksImpl;

import java.lang.reflect.InvocationTargetException;

/** Holds and provides an instance of {@link ProcessScope}. */
public final class XSurfaceProcessScopeProvider {
    private static ProcessScope sProcessScope;

    public static ProcessScope getProcessScope() {
        if (sProcessScope != null) {
            return sProcessScope;
        }
        XSurfaceHooks hooks = XSurfaceHooksImpl.getInstance();
        if (!hooks.isEnabled()) {
            return null;
        }
        sProcessScope = hooks.createProcessScope(getDependencyProviderFactory().create());
        return sProcessScope;
    }

    private static ProcessScopeDependencyProviderFactory getDependencyProviderFactory() {
        Class<?> dependencyProviderFactoryClazz;
        try {
            dependencyProviderFactoryClazz =
                    Class.forName("org.chromium.chrome.browser.app.xsurface_provider."
                            + "ProcessScopeDependencyProviderFactoryImpl");
        } catch (ClassNotFoundException e) {
            return null;
        }
        try {
            return (ProcessScopeDependencyProviderFactory) dependencyProviderFactoryClazz
                    .getDeclaredMethod("getInstance")
                    .invoke(null);
        } catch (NoSuchMethodException e) {
        } catch (InvocationTargetException e) {
        } catch (IllegalAccessException e) {
        }
        return null;
    }

    public static void setProcessScopeForTesting(ProcessScope processScope) {
        sProcessScope = processScope;
    }
}
