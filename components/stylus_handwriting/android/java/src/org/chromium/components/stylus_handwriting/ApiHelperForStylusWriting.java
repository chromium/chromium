// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;

import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Helper class to determine whether Direct writing service is in consideration or the Android
 * platform Stylus Writing feature, and to set the appropriate handler to WebContents.
 */
public class ApiHelperForStylusWriting {
    /**
     * Notifies the applicable {@link StylusWritingHandler} so that stylus writing messages can be
     * received and handled for performing handwriting recognition.
     *
     * @param webContents current web contents
     */
    public static void onWebContentsInitialized(WebContents webContents) {
        ViewAndroidDelegate viewAndroidDelegate = webContents.getViewAndroidDelegate();
        if (viewAndroidDelegate == null) return;
        DirectWritingTrigger.getInstance().updateDWSettings(
                viewAndroidDelegate.getContainerView().getContext());
        if (DirectWritingTrigger.getInstance().isStylusWritingEnabled()) {
            DirectWritingTrigger.getInstance().onWebContentsInitialized(webContents);
        }
        // TODO(mahesh.ma): Add Android platform stylus writing logic here.
    }

    /**
     * Notify window focus has changed
     *
     * @param hasFocus whether window gained or lost focus
     * @param context current {@link Context}
     */
    public static void onWindowFocusChanged(boolean hasFocus, Context context) {
        // This notification is used to determine if the Stylus writing feature is enabled or not
        // from System settings as it can be changed while Chrome is in background.
        // TODO(crbug.com/1340944): Update the stylus writing feature status correctly in open tab.
        DirectWritingTrigger.getInstance().onWindowFocusChanged(hasFocus, context);
    }
}
