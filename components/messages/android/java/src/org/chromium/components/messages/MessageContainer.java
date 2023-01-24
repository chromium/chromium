// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

/**
 * Container holding messages.
 */
public class MessageContainer extends FrameLayout {
    private static final String TAG = "MessageContainer";

    interface MessageContainerA11yDelegate {
        void onA11yFocused();
        void onA11yFocusCleared();
        void onA11yDismiss();
    }

    private MessageContainerA11yDelegate mA11yDelegate;
    private boolean mIsInitializingLayout;

    public MessageContainer(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityEvent(
                    @NonNull View host, @NonNull AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                    if (mA11yDelegate != null) mA11yDelegate.onA11yFocused();
                } else if (event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
                    if (mA11yDelegate != null) mA11yDelegate.onA11yFocusCleared();
                }
                super.onInitializeAccessibilityEvent(host, event);
            }
        });
        ViewCompat.replaceAccessibilityAction(
                this, AccessibilityActionCompat.ACTION_DISMISS, null, (v, c) -> {
                    if (mA11yDelegate != null) {
                        mA11yDelegate.onA11yDismiss();
                        return true;
                    }
                    return false;
                });
    }

    /**
     * Show a given message view on the screen. There should be no more than one unique view
     * before adding a message.
     * @param view The message view to display on the screen.
     */
    void addMessage(View view) {
        if (indexOfChild(view) != -1) {
            throw new IllegalStateException("Should not contain the target view when adding.");
        }
        int index = 0;
        if (MessageFeatureList.isStackAnimationEnabled()) {
            if (getChildCount() > 1) {
                throw new IllegalStateException(
                        "Should not contain more than 2 views when adding a new message.");
            } else if (getChildCount() == 1) {
                View cur = getChildAt(0);
                index = cur.getElevation() > view.getElevation() ? 1 : 0;
            }
        } else if (getChildCount() == 1) {
            throw new IllegalStateException(
                    "Should not contain any view when adding a new message.");
        }
        super.addView(view, index);

        // TODO(crbug.com/1178965): clipChildren should be set to false only when the message is in
        // motion.
    }

    /**
     * Remove the given message view, which is being shown inside the container.
     * @param view The message which should be removed.
     */
    void removeMessage(View view) {
        if (indexOfChild(view) == -1) {
            throw new IllegalStateException("The given view is not being shown.");
        }
        super.removeView(view);
        if (getChildCount() == 0) {
            mA11yDelegate = null;
        }
    }

    public int getMessageBannerHeight() {
        assert getChildCount() > 0;
        // TODO(https://crbug.com/1382275): remove this log after fix.
        if (getChildAt(0) == null) {
            Log.w(TAG, "Null child in message container; child count %s", getChildCount());
        }
        return getChildAt(0).getHeight();
    }

    public int getMessageShadowTopMargin() {
        return getResources().getDimensionPixelOffset(R.dimen.message_shadow_top_margin);
    }

    @Override
    public void setLayoutParams(ViewGroup.LayoutParams params) {
        try (TraceEvent e = TraceEvent.scoped("MessageContainer.setLayoutParams")) {
            super.setLayoutParams(params);
        }
    }

    void setA11yDelegate(MessageContainerA11yDelegate a11yDelegate) {
        mA11yDelegate = a11yDelegate;
    }

    View getSiblingView(View current) {
        assert getChildCount() > 1;
        int idx = indexOfChild(current);
        assert idx != -1;
        return getChildAt(1 - idx);
    }

    /**
     * Runs a {@link Runnable} after the message's initial layout. If the view is already laid out,
     * the {@link Runnable} will be called immediately.
     * @param runnable The {@link Runnable}.
     */
    void runAfterInitialMessageLayout(Runnable runnable) {
        View view = getChildAt(0);
        assert view != null;
        if (view.getHeight() > 0) {
            mIsInitializingLayout = false;
            runnable.run();
            return;
        }

        mIsInitializingLayout = true;
        view.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v.getHeight() == 0) return;

                runnable.run();
                v.removeOnLayoutChangeListener(this);
                mIsInitializingLayout = false;
            }
        });
    }

    /**
     * Returns whether container is initializing its layout for a new added view. Clients should not
     * call {@link #runAfterInitialMessageLayout(Runnable)} when it returns true.
     * @return True if it is initializing layout.
     */
    public boolean isIsInitializingLayout() {
        return mIsInitializingLayout;
    }

    /**
     * Call {@link #addMessage(View)} instead in order to prevent from uncontrolled add.
     */
    @Override
    @Deprecated
    public final void addView(View view) {
        throw new RuntimeException("Use addMessage instead.");
    }

    /**
     * Call {@link #removeMessage(View)} instead in order to prevent from uncontrolled remove.
     */
    @Override
    @Deprecated
    public final void removeView(View view) {
        throw new RuntimeException("Use removeMessage instead.");
    }
}
