// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_state;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Provides a way of accessing helpers for page security state. */
public class SecurityStateModel {
    /**
     * Fetch the security level for a given web contents.
     *
     * @param webContents The web contents to get the security level for.
     * @return The ConnectionSecurityLevel for the specified web contents.
     *
     * @see ConnectionSecurityLevel
     */
    public static int getSecurityLevelForWebContents(WebContents webContents) {
        if (webContents == null) return ConnectionSecurityLevel.NONE;
        return SecurityStateModelJni.get().getSecurityLevelForWebContents(webContents);
    }

    public static boolean isContentDangerous(WebContents webContents) {
        return getSecurityLevelForWebContents(webContents) == ConnectionSecurityLevel.DANGEROUS;
    }

    private SecurityStateModel() {}

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        int getSecurityLevelForWebContents(WebContents webContents);
    }
}
