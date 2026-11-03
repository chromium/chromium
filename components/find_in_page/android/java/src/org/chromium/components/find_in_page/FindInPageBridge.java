// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Allows issuing find in page related requests for a given WebContents. */
@JNINamespace("find_in_page")
@NullMarked
public class FindInPageBridge {
    @JniType("::find_in_page::FindInPageBridge")
    interface NativeFindInPageBridge extends JniTypeToken {}

    private final JniUniquePtr<NativeFindInPageBridge> mNativeFindInPageBridge;

    public FindInPageBridge(WebContents webContents) {
        assert webContents != null;
        mNativeFindInPageBridge = FindInPageBridgeJni.get().init(this, webContents);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        mNativeFindInPageBridge.destroy();
    }

    /**
     * Starts the find operation by calling StartFinding on the ChromeTab.
     * This function does not block while a search is in progress.
     * Set a listener using setFindResultListener to receive the results.
     */
    public void startFinding(String searchString, boolean forwardDirection, boolean caseSensitive) {
        FindInPageBridgeJni.get()
                .startFinding(
                        mNativeFindInPageBridge, searchString, forwardDirection, caseSensitive);
    }

    /**
     * When the user commits to a search query or jumps from one result to the next, move
     * accessibility focus to the next find result.
     */
    public void activateFindInPageResultForAccessibility() {
        FindInPageBridgeJni.get().activateFindInPageResultForAccessibility(mNativeFindInPageBridge);
    }

    /**
     * Stops the current find operation.
     *
     * @param clearSelection Whether the selection on the page should be cleared.
     */
    public void stopFinding(boolean clearSelection) {
        FindInPageBridgeJni.get().stopFinding(mNativeFindInPageBridge, clearSelection);
    }

    /** Returns the most recent find text before the current one. */
    public String getPreviousFindText() {
        return FindInPageBridgeJni.get().getPreviousFindText(mNativeFindInPageBridge);
    }

    /** Asks the renderer to send the bounding boxes of current find matches. */
    public void requestFindMatchRects(int currentVersion) {
        FindInPageBridgeJni.get().requestFindMatchRects(mNativeFindInPageBridge, currentVersion);
    }

    /**
     * Selects and zooms to the nearest find result to the point (x,y), where x and y are fractions
     * of the content document's width and height.
     */
    public void activateNearestFindResult(float x, float y) {
        FindInPageBridgeJni.get().activateNearestFindResult(mNativeFindInPageBridge, x, y);
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeFindInPageBridge> init(FindInPageBridge self, WebContents webContents);

        void startFinding(
                JniPtr<NativeFindInPageBridge> self,
                String searchString,
                boolean forwardDirection,
                boolean caseSensitive);

        void stopFinding(JniPtr<NativeFindInPageBridge> self, boolean clearSelection);

        String getPreviousFindText(JniPtr<NativeFindInPageBridge> self);

        void requestFindMatchRects(JniPtr<NativeFindInPageBridge> self, int currentVersion);

        void activateNearestFindResult(JniPtr<NativeFindInPageBridge> self, float x, float y);

        void activateFindInPageResultForAccessibility(JniPtr<NativeFindInPageBridge> self);
    }
}
