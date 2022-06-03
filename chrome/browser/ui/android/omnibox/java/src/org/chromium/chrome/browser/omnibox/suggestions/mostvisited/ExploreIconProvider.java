// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

/** The provider of the explore sites icon. */
public interface ExploreIconProvider {
    /**
     * @param pixelSize The icon width in pixel.
     * @param callback Called when the icon is available.
     */
    void getSummaryImage(int pixelSize, Callback<Bitmap> callback);
}
