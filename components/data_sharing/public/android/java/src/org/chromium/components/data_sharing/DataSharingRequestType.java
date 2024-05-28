// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.IntDef;

/** Enumeration for request types for DataSharingService APIs. */
@IntDef({
    DataSharingRequestType.CREATE_GROUP,
    DataSharingRequestType.READ_GROUPS,
    DataSharingRequestType.READ_ALL_GROUPS,
    DataSharingRequestType.UPDATE_GROUP,
    DataSharingRequestType.DELETE_GROUPS,
    DataSharingRequestType.TEST_REQUEST,
})
public @interface DataSharingRequestType {
    /** Create group request. */
    int CREATE_GROUP = 0;

    /** Read groups request. */
    int READ_GROUPS = 1;

    /** Read all groups request. */
    int READ_ALL_GROUPS = 2;

    /** Update group request. */
    int UPDATE_GROUP = 3;

    /** Delete groups request. */
    int DELETE_GROUPS = 4;

    /** For tests */
    int TEST_REQUEST = 5;
}
