// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.chromium.ui.base.WindowAndroid;

/**
 * A factory for producing a {@link PwaBottomSheetController}.
 */
public class PwaBottomSheetControllerFactory {
    public static PwaBottomSheetController createPwaBottomSheetController(Activity activity) {
        return new PwaBottomSheetController(activity);
    }

    public static void attach(WindowAndroid windowAndroid, PwaBottomSheetController controller) {
        PwaBottomSheetControllerProvider.attach(windowAndroid, controller);
    }

    public static void detach(PwaBottomSheetController controller) {
        PwaBottomSheetControllerProvider.detach(controller);
    }
}
