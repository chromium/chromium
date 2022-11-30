// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

/**
 * Callbacks when permission changes.
 */
public interface AssistantQrCodePermissionCallback {
    /** Called when hasPermission changes. */
    void onPermissionsChanged(boolean hasPermission);
}