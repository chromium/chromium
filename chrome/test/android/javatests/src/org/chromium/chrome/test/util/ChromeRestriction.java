// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

/**
 * Restrictions that are usable with the @Restriction enum but in the chrome/ layer.
 * e.g. @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE})
 */
public final class ChromeRestriction {
    /** Specifies the test is only valid on a device that has up to date play services. */
    public static final String RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES = "Google_Play_Services";
    /** Specifies the test is only valid on official build. */
    public static final String RESTRICTION_TYPE_OFFICIAL_BUILD = "Official_Build";
    /** Specifies the test is only valid on a Daydream-ready device */
    public static final String RESTRICTION_TYPE_DEVICE_DAYDREAM = "Daydream_Ready";
    /** Specifies the test is only valid on a non-Daydream-ready device */
    public static final String RESTRICTION_TYPE_DEVICE_NON_DAYDREAM = "Non_Daydream_Ready";
    /** Specifies the test is only valid if the current VR viewer is Daydream View */
    public static final String RESTRICTION_TYPE_VIEWER_DAYDREAM = "Daydream_View";
    /** Specifies the test is only valid if the current VR viewer is not Daydream View */
    public static final String RESTRICTION_TYPE_VIEWER_NON_DAYDREAM = "Non_Daydream_View";
    /** Specifies the test is only valid if the VR DON flow is enabled */
    public static final String RESTRICTION_TYPE_VR_DON_ENABLED = "VR_DON_Enabled";
}
