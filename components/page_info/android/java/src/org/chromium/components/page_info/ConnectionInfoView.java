// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Java side of Android implementation of the page info UI.
 */
public class ConnectionInfoView {

    /**
     * Adds certificate section, which contains an icon, a headline, a
     * description and a label for certificate info link.
     */
    @CalledByNative
    private void addCertificateSection(
            int iconId, String headline, String description, String label, int iconColorId) {
    }

    /**
     * Adds Description section, which contains an icon, a headline, and a
     * description. Most likely headline for description is empty
     */
    @CalledByNative
    private void addDescriptionSection(
            int iconId, String headline, String description, int iconColorId) {
    }

    @CalledByNative
    private void addResetCertDecisionsButton(String label) {
    }

    @CalledByNative
    private void addMoreInfoLink(String linkText) {
    }

    /** Displays the ConnectionInfoView. */
    @CalledByNative
    private void onReady() {
    }

    @NativeMethods
    interface Natives {
        long init(ConnectionInfoView popup, WebContents webContents);
        void destroy(long nativeConnectionInfoViewAndroid, ConnectionInfoView caller);
        void resetCertDecisions(long nativeConnectionInfoViewAndroid, ConnectionInfoView caller,
                WebContents webContents);
    }
}
