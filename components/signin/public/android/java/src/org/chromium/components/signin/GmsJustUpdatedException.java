// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 */
public class GmsJustUpdatedException extends AccountManagerDelegateException {
    public GmsJustUpdatedException(String message) {
        super(message);
    }

    public GmsJustUpdatedException(GmsJustUpdatedException cause) {
        super(cause);
    }
}
