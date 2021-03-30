// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import com.google.android.gms.common.GoogleApiAvailability;

/**
 * This class encapsulates return code if GMSCore package is not available.
 */
public class GmsAvailabilityException extends AccountManagerDelegateException {
    private final int mResultCode;

    public GmsAvailabilityException(String message, int resultCode) {
        super(message);
        mResultCode = resultCode;
    }

    public GmsAvailabilityException(GmsAvailabilityException cause) {
        super(cause);
        mResultCode = cause.mResultCode;
    }

    public int getGmsAvailabilityReturnCode() {
        return mResultCode;
    }

    public boolean isUserResolvableError() {
        return GoogleApiAvailability.getInstance().isUserResolvableError(mResultCode);
    }
}
