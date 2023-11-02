// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import android.view.ViewStub;

import org.chromium.chrome.browser.video_tutorials.Tutorial;

/**
 * Creates and shows a video tutorial IPH. Requires a {@link ViewStub} to be passed which will
 * inflate when the IPH is shown.
 */
public interface VideoIPHCoordinator {
    /**
     * Shows an IPH card featuring the given {@link Tutorial}. Can be called multiple times with
     * different tutorials. The ViewStub will inflate and create the card on the first invocation.
     * @param tutorial The tutorial to be featured in the IPH.
     */
    void showVideoIPH(Tutorial tutorial);

    /**
     * Hides the IPH card. Should be called after it has been clicked.
     */
    void hideVideoIPH();
}