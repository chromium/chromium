// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.CronetException;

/** Implements {@link CronetException}. */
public class CronetExceptionImpl extends CronetException {
    public CronetExceptionImpl(String message, Throwable cause) {
        super(message, cause);
    }
}
