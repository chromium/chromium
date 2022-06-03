// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class for obtaining site certificate chain from WebContents.
 */
public class CertificateChainHelper {
    public static byte[][] getCertificateChain(WebContents webContents) {
        return CertificateChainHelperJni.get().getCertificateChain(webContents);
    }

    @NativeMethods
    interface Natives {
        byte[][] getCertificateChain(WebContents webContents);
    }
}
