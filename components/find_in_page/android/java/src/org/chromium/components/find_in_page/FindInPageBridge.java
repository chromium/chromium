// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Allows issuing find in page related requests for a given WebContents. */
@JNINamespace("find_in_page")
@NullMarked
public class FindInPageBridge {
    private long mNativeFindInPageBridge;

    public FindInPageBridge(WebContents webContents) {
        assert webContents != null;
        mNativeFindInPageBridge = FindInPageBridgeJni.get().init(this, webContents);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        FindInPageBridgeJni.get().destroy(mNativeFindInPageBridge);
        mNativeFindInPageBridge = 0;
    }

    /**
     * Starts the find operation by calling StartFinding on the ChromeTab.
     * This function does not block while a search is in progress.
     * Set a listener using setFindResultListener to receive the results.
     */
    public void startFinding(String searchString, boolean forwardDirection, boolean caseSensitive) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get()
                .startFinding(
                        mNativeFindInPageBridge, searchString, forwardDirection, caseSensitive);
    }

    /**
     * When the user commits to a search query or jumps from one result to the next, move
     * accessibility focus to the next find result.
     */
    public void activateFindInPageResultForAccessibility() {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get().activateFindInPageResultForAccessibility(mNativeFindInPageBridge);
    }

    /**
     * Stops the current find operation.
     *
     * @param clearSelection Whether the selection on the page should be cleared.
     */
    public void stopFinding(boolean clearSelection) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get().stopFinding(mNativeFindInPageBridge, clearSelection);
    }

    /** Returns the most recent find text before the current one. */
    public String getPreviousFindText() {
        assert mNativeFindInPageBridge != 0;
        return FindInPageBridgeJni.get().getPreviousFindText(mNativeFindInPageBridge);
    }

    /** Asks the renderer to send the bounding boxes of current find matches. */
    public void requestFindMatchRects(int currentVersion) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get().requestFindMatchRects(mNativeFindInPageBridge, currentVersion);
    }

    /**
     * Selects and zooms to the nearest find result to the point (x,y), where x and y are fractions
     * of the content document's width and height.
     */
    public void activateNearestFindResult(float x, float y) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get().activateNearestFindResult(mNativeFindInPageBridge, x, y);
    }

    @NativeMethods
    interface Natives {
        long init(FindInPageBridge self, WebContents webContents);

        void destroy(long nativeFindInPageBridge);

        void startFinding(
                long nativeFindInPageBridge,
                String searchString,
                boolean forwardDirection,
                boolean caseSensitive);

        void stopFinding(long nativeFindInPageBridge, boolean clearSelection);

        String getPreviousFindText(long nativeFindInPageBridge);

        void requestFindMatchRects(long nativeFindInPageBridge, int currentVersion);

        void activateNearestFindResult(long nativeFindInPageBridge, float x, float y);

        void activateFindInPageResultForAccessibility(long nativeFindInPageBridge);
    }
}
