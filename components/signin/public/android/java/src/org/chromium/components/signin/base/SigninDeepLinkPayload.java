// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Payload of the signin deep link.
 *
 * <p>This class is a model for the C++ class
 * (components/signin/public/base/signin_deep_link_payload.h), and the C++ class is the source of
 * truth.
 */
@NullMarked
public final class SigninDeepLinkPayload {

    private final @ExternalEntryPoint int mExternalEntryPoint;
    private final String mEmail;

    @CalledByNative
    public SigninDeepLinkPayload(
            @ExternalEntryPoint int externalEntryPoint, @JniType("std::string") String email) {
        mExternalEntryPoint = externalEntryPoint;
        mEmail = email;
    }

    /** Returns the external device entry point of the signin deep link. */
    @ExternalEntryPoint
    public int getExternalEntryPoint() {
        return mExternalEntryPoint;
    }

    /** Returns the email address of the signin deep link. */
    public String getEmail() {
        return mEmail;
    }
}
