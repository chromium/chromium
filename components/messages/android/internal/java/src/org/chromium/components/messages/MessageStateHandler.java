// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface of a handler to show, hide or dismiss the message. */
public interface MessageStateHandler {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Position.INVISIBLE, Position.FRONT, Position.BACK})
    @interface Position {
        int INVISIBLE = 0;
        int FRONT = 1;
        int BACK = 2;
    }

    /**
     * Signals that the message needs to show its UI.
     *
     * @param fromIndex The position from which the message view starts to move.
     * @param toIndex The target position at which the message view animation will stop.
     * @return The animator to trigger the showing animation.
     */
    @NonNull
    Animator show(@Position int fromIndex, @Position int toIndex);

    /**
     * Signals that the message needs to hide its UI.
     *
     * @param fromIndex The position from which the message view starts to move.
     * @param toIndex The target position at which the message view animation will stop.
     * @param animate Whether animation should be run or not.
     * @return The animator to trigger the hiding animation.
     */
    @Nullable
    Animator hide(@Position int fromIndex, @Position int toIndex, boolean animate);

    /**
     * Notify that the message is about to be dismissed from the queue.
     *
     * @param dismissReason The reason why the message is being dismissed.
     */
    void dismiss(@DismissReason int dismissReason);

    /** Returns MessageIdentifier of the current message. */
    @MessageIdentifier
    int getMessageIdentifier();
}
