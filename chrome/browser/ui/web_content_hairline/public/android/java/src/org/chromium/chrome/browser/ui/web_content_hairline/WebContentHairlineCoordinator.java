// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_content_hairline;

import org.chromium.build.annotations.NullMarked;

/**
 * Public interface for the WebContents hairline, which draws the hairlines (and rounded corners)
 * separating the WebContents from the top controls and the side UIs surrounding it.
 */
@NullMarked
public interface WebContentHairlineCoordinator {

    /** Destroys all owned objects. */
    void destroy();
}
