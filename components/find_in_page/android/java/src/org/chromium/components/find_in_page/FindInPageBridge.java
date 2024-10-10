// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Allows issuing find in page related requests for a given WebContents. */
@JNINamespace("find_in_page")
public class FindInPageBridge {
    private long mNativeFindInPageBridge;

    public FindInPageBridge(WebContents webContents) {
        assert webContents != null;
        mNativeFindInPageBridge =
                FindInPageBridgeJni.get().init(FindInPageBridge.this, webContents);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        FindInPageBridgeJni.get().destroy(mNativeFindInPageBridge, FindInPageBridge.this);
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
                        mNativeFindInPageBridge,
                        FindInPageBridge.this,
                        searchString,
                        forwardDirection,
                        caseSensitive);
    }

    /**
     * When the user commits to a search query or jumps from one result
     * to the next, move accessibility focus to the next find result.
     */
    public void activateFindInPageResultForAccessibility() {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get()
                .activateFindInPageResultForAccessibility(
                        mNativeFindInPageBridge, FindInPageBridge.this);
    }

    /**
     * Stops the current find operation.
     * @param clearSelection Whether the selection on the page should be cleared.
     * */
    public void stopFinding(boolean clearSelection) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get()
                .stopFinding(mNativeFindInPageBridge, FindInPageBridge.this, clearSelection);
    }

    /** Returns the most recent find text before the current one. */
    public String getPreviousFindText() {
        assert mNativeFindInPageBridge != 0;
        return FindInPageBridgeJni.get()
                .getPreviousFindText(mNativeFindInPageBridge, FindInPageBridge.this);
    }

    /** Asks the renderer to send the bounding boxes of current find matches. */
    public void requestFindMatchRects(int currentVersion) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get()
                .requestFindMatchRects(
                        mNativeFindInPageBridge, FindInPageBridge.this, currentVersion);
    }

    /**
     * Selects and zooms to the nearest find result to the point (x,y),
     * where x and y are fractions of the content document's width and height.
     */
    public void activateNearestFindResult(float x, float y) {
        assert mNativeFindInPageBridge != 0;
        FindInPageBridgeJni.get()
                .activateNearestFindResult(mNativeFindInPageBridge, FindInPageBridge.this, x, y);
    }

    @NativeMethods
    interface Natives {
        long init(FindInPageBridge caller, WebContents webContents);

        void destroy(long nativeFindInPageBridge, FindInPageBridge caller);

        void startFinding(
                long nativeFindInPageBridge,
                FindInPageBridge caller,
                String searchString,
                boolean forwardDirection,
                boolean caseSensitive);

        void stopFinding(
                long nativeFindInPageBridge, FindInPageBridge caller, boolean clearSelection);

        String getPreviousFindText(long nativeFindInPageBridge, FindInPageBridge caller);

        void requestFindMatchRects(
                long nativeFindInPageBridge, FindInPageBridge caller, int currentVersion);

        void activateNearestFindResult(
                long nativeFindInPageBridge, FindInPageBridge caller, float x, float y);

        void activateFindInPageResultForAccessibility(
                long nativeFindInPageBridge, FindInPageBridge caller);
    }
}
