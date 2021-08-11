// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Message definitions passed over the Eche privileged/unprivileged pipe.
 */

/**
 * Representation of an event passed in from the phone notification.
 * @typedef {{
 *    notificationId: string,
 *    packageName: string,
 *    timestamp: number,
 * }}
 */
/* #export */ let NotificationInfo;

/**
 * Representation of the system info passed in from SystemInfoProvider.
 * @typedef {{
 *    boardName: string,
 *    deviceName: string,
 * }}
 */
/* #export */ let SystemInfo;

/**
 * Representation of the uid from local device.
 * @typedef {{
 *    localUid: string,
 * }}
 */
/* #export */ let UidInfo;

/**
 * Enum for message types.
 * @enum {string}
 */
 /* #export */ const Message = {
    // Message for sending window close request to privileged section.
    CLOSE_WINDOW: 'close-window',
    // Message for sending signaling data in bi-directional pipes.
    SEND_SIGNAL: 'send-signal',
    // Message for sending tear down signal request to privileged section.
    TEAR_DOWN_SIGNAL: 'tear-down-signal',
    // Message for getting the result of getSystemInfo api from privileged
    // section.
    GET_SYSTEM_INFO: 'get-system-info',
    // Message for getting the result of getUid api from privileged section.
    GET_UID: 'get-uid',
    // Message for sending screen backlight state to unprivileged section.
    SCREEN_BACKLIGHT_STATE: 'screen-backlight-state',
    // Message for sending tablet mode state to unprivileged section.
    TABLET_MODE: 'tablet-mode',
    // Message for sending notification event to unprivileged section.
    NOTIFICATION_INFO: 'notification_info',
};