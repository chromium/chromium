// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Container holding messages.
 */
public class MessageContainer extends FrameLayout {
    public MessageContainer(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Show a given message view on the screen. There should be no view inside the container
     * before adding a message.
     * @param view The message view to display on the screen.
     */
    void addMessage(View view) {
        if (getChildCount() != 0) {
            throw new IllegalStateException(
                    "Should not contain any view when adding a new message.");
        }
        addView(view);
    }

    /**
     * Hide the given message view, which is being shown inside the container.
     * @param view The message which should be removed.
     */
    void removeMessage(View view) {
        if (getChildCount() == 0) {
            throw new IllegalStateException("The given view is not being shown.");
        }
        removeAllViews();
    }
}
