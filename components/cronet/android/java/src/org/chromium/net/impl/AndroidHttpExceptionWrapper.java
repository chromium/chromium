// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.HttpException;

import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetException;

// Note we specify both RequiresApi and RequiresExtension because some older linters may only
// recognize the former.
@RequiresApi(EXT_API_LEVEL)
@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidHttpExceptionWrapper extends CronetException {
    AndroidHttpExceptionWrapper(HttpException e) {
        super(e.getMessage(), e);
    }
}
