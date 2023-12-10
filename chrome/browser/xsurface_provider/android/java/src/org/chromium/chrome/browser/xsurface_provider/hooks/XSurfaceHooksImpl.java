// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider.hooks;

/** A stub XSurfaceHooks implementation. Replace at build time on internal builds. */
public class XSurfaceHooksImpl implements XSurfaceHooks {
    private static XSurfaceHooks sXSurfaceHooks;

    public static XSurfaceHooks getInstance() {
        if (sXSurfaceHooks == null) {
            sXSurfaceHooks = new XSurfaceHooksImpl() {};
        }
        return sXSurfaceHooks;
    }

    public static void setInstanceForTesting(XSurfaceHooks xSurfaceHooks) {
        sXSurfaceHooks = xSurfaceHooks;
    }
}
