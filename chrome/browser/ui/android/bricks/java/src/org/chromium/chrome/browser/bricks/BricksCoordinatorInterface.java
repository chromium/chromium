// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks;

import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Interface for the Bricks coordinator in the DFM. */
@NullMarked
public interface BricksCoordinatorInterface {
    View getView();

    void destroy();
}
