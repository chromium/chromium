// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.viz.common;

/**
 * Contains command line switches that are specific to the viz/* portion of Chromium on Android.
 */
public abstract class VizSwitches {
    // Enables experimental de-jelly effect.
    public static final String ENABLE_DE_JELLY = "enable-de-jelly";

    // Prevent instantiation.
    private VizSwitches() {}
}
