// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public interface DataSharingSDKDelegateProtoResponseCallback {

    // Determines the Status of the response.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Status.SUCCESS, Status.FAILURE})
    @interface Status {
        int SUCCESS = 0;
        int FAILURE = 1;
    }

    // Callback Method
    void run(byte[] serializedProto, @Status int status);
}
