// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.wallet;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.List;

/** Provides native methods for java code to call. */
@JNINamespace("wallet")
public class BoardingPassBridge {

    private BoardingPassBridge() {}

    /**
     * Decides whether to run boarding pass detection on given url.
     *
     * @param url URL of the current page.
     * @return whether to run detection
     */
    public static boolean shouldDetect(String url) {
        return BoardingPassBridgeJni.get().shouldDetect(url);
    }

    /**
     * Detects boarding pass barcode raw string from web page.
     *
     * @param webContents web contents of current tab.
     * @return Detected boarding passes raw string.
     */
    public static Promise<List<String>> detectBoardingPass(WebContents webContents) {
        Promise<List<String>> promise = new Promise<>();
        BoardingPassBridgeJni.get()
                .detectBoardingPass(
                        webContents,
                        boardingPasses -> {
                            if (!promise.isRejected()) {
                                promise.fulfill(Arrays.asList(boardingPasses));
                            }
                        });
        return promise;
    }

    @NativeMethods
    public interface Natives {
        boolean shouldDetect(String url);

        void detectBoardingPass(WebContents webContents, Callback<String[]> callback);
    }
}
