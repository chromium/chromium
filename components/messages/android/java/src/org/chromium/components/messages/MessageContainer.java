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

import org.chromium.ui.base.ViewUtils;

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
        // TODO(sinansahin): clipChildren should be set to false only when the message is in motion.
        ViewUtils.setAncestorsShouldClipChildren(this, false);
    }

    /**
     * Remove the given message view, which is being shown inside the container.
     * @param view The message which should be removed.
     */
    void removeMessage(View view) {
        if (indexOfChild(view) < 0) {
            throw new IllegalStateException("The given view is not being shown.");
        }
        ViewUtils.setAncestorsShouldClipChildren(this, true);
        removeAllViews();
    }

    /**
     * Runs a {@link Runnable} after the initial layout. If the view is already laid out, the
     * {@link Runnable} will be called immediately.
     * @param runnable The {@link Runnable}.
     */
    void runAfterInitialLayout(Runnable runnable) {
        if (getHeight() > 0) {
            runnable.run();
            return;
        }

        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v.getHeight() == 0) return;

                runnable.run();
                v.removeOnLayoutChangeListener(this);
            }
        });
    }
}
