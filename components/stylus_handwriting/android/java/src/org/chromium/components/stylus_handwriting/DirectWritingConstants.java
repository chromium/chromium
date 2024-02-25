// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

/**
 * Class to store Direct writing constants. The service package constants are from Samsung platform
 * and the MSG_ constants are used to identify Service callback command messages.
 */
class DirectWritingConstants {
    private DirectWritingConstants() {}

    // This constant is defined in Samsung Platform, which sets the Hover icon for direct writing.
    // Since this constant is not available to non-Samsung apps, it is defined below.
    static final int STYLUS_WRITING_ICON_VALUE = 20024;

    static final String SERVICE_PKG_NAME = "com.samsung.android.honeyboard";
    static final String SERVICE_CLS_NAME =
            "com.samsung.android.directwriting.service.DirectWritingService";

    // The fingerprints of valid Samsung Direct Writing service package.
    static final String SERVICE_PKG_SHA_256_FINGERPRINT_DEBUG =
            "C8:A2:E9:BC:CF:59:7C:2F:B6:DC:66:BE:E2:93:FC:13"
                    + ":F2:FC:47:EC:77:BC:6B:2B:0D:52:C1:1F:51:19:2A:B8";
    static final String SERVICE_PKG_SHA_256_FINGERPRINT_RELEASE =
            "34:DF:0E:7A:9F:1C:F1:89:2E:45:C0:56:B4:97:3C:D8"
                    + ":1C:CF:14:8A:40:50:D1:1A:EA:4A:C5:A6:5F:90:0A:42";

    /** Set text and selection from service callback */
    static final int MSG_SEND_SET_TEXT_SELECTION = 101;

    /** Do perform Editor Action from service callback */
    static final int MSG_PERFORM_EDITOR_ACTION = 201;

    /** Update Edit field bounds to service when requested from service callback */
    public static final int MSG_UPDATE_EDIT_BOUNDS = 202;

    /** Do perform show keyboard via ImeAdapter */
    static final int MSG_PERFORM_SHOW_KEYBOARD = 301;

    /** Force hide keyboard command from service callback */
    static final int MSG_FORCE_HIDE_KEYBOARD = 302;

    /** Do perform extra text view command from Service callback */
    static final int MSG_TEXT_VIEW_EXTRA_COMMAND = 401;
}
