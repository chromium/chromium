// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

/** A listener for the InfoBar animations. */
public interface InfoBarAnimationListener {
    public static final int ANIMATION_TYPE_SHOW = 0;
    public static final int ANIMATION_TYPE_SWAP = 1;
    public static final int ANIMATION_TYPE_HIDE = 2;

    /** Notifies the subscriber when an animation is completed. */
    void notifyAnimationFinished(int animationType);

    /**
     * Notifies the subscriber when all animations are finished.
     * @param frontInfoBar The frontmost infobar or {@code null} if none are showing.
     */
    void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar);
}
