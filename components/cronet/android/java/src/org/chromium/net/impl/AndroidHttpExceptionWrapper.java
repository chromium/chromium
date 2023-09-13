// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpException;

import androidx.annotation.RequiresApi;

import org.chromium.net.CronetException;

@RequiresApi(api = 34)
class AndroidHttpExceptionWrapper extends CronetException {
    AndroidHttpExceptionWrapper(HttpException e) {
        super(e.getMessage(), e);
    }
}
