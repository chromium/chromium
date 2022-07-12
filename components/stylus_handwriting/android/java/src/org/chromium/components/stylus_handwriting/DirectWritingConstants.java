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
