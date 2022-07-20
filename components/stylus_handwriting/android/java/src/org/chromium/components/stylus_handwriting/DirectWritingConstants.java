// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

/**
 * Class to store Direct writing constants. The service package constants are from Samsung platform
 * and the MSG_ constants are used to identify Service callback command messages.
 */
class DirectWritingConstants {
    private DirectWritingConstants() {}

    static final String SERVICE_PKG_NAME = "com.samsung.android.honeyboard";
    static final String SERVICE_CLS_NAME =
            "com.samsung.android.directwriting.service.DirectWritingService";

    // The fingerprint of valid Samsung Direct Writing service package.
    static final String SERVICE_PKG_SHA_256_FINGERPRINT =
            "C8:A2:E9:BC:CF:59:7C:2F:B6:DC:66:BE:E2:93:FC:13"
            + ":F2:FC:47:EC:77:BC:6B:2B:0D:52:C1:1F:51:19:2A:B8";

    /**
     * Set text and selection from service callback
     */
    static final int MSG_SEND_SET_TEXT_SELECTION = 101;

    /**
     * Do perform Editor Action from service callback
     */
    static final int MSG_PERFORM_EDITOR_ACTION = 201;

    /**
     * Do perform show keyboard via ImeAdapter
     */
    static final int MSG_PERFORM_SHOW_KEYBOARD = 301;

    /**
     * Force hide keyboard command from service callback
     */
    static final int MSG_FORCE_HIDE_KEYBOARD = 302;

    /**
     * Do perform extra text view command from Service callback
     */
    static final int MSG_TEXT_VIEW_EXTRA_COMMAND = 401;
}
