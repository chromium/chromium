// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fakepdf;

import android.content.Context;

import androidx.annotation.NonNull;

/** Fake PDF viewer API. Will be removed once real APIs become available. */
public class ProjectorContext {
    public static void installProjectorGlobalsForTest(@NonNull Context appContext) {}
}
