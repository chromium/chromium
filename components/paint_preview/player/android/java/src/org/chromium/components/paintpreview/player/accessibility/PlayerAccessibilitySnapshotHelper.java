// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.accessibility;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;

/**
 * This class helps with populating {@link AccessibilitySnapshotNode} from native AXTreeUpdate.
 */
public class PlayerAccessibilitySnapshotHelper {
    public static AccessibilitySnapshotNode getJavaAccessibilitySnapshotNode(long nativeAxTree) {
        return PlayerAccessibilitySnapshotHelperJni.get().getAccessibilitySnapshot(nativeAxTree);
    }

    @CalledByNative
    private static AccessibilitySnapshotNode createAccessibilitySnapshotNode(int parentRelativeLeft,
            int parentRelativeTop, int width, int height, boolean isRootNode, String text,
            int color, int bgcolor, float size, boolean bold, boolean italic, boolean underline,
            boolean lineThrough, String className) {
        AccessibilitySnapshotNode node = new AccessibilitySnapshotNode(text, className);

        // if size is smaller than 0, then style information does not exist.
        if (size >= 0.0) {
            node.setStyle(color, bgcolor, size, bold, italic, underline, lineThrough);
        }
        node.setLocationInfo(parentRelativeLeft, parentRelativeTop, width, height, isRootNode);
        return node;
    }

    @CalledByNative
    private static void setAccessibilitySnapshotSelection(
            AccessibilitySnapshotNode node, int start, int end) {
        node.setSelection(start, end);
    }

    @CalledByNative
    private static void addAccessibilityNodeAsChild(
            AccessibilitySnapshotNode parent, AccessibilitySnapshotNode child) {
        parent.addChild(child);
    }

    @NativeMethods
    interface Natives {
        AccessibilitySnapshotNode getAccessibilitySnapshot(long axTree);
    }
}
