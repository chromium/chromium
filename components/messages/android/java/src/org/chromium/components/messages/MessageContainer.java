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

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

/** Container holding messages. */
public class MessageContainer extends FrameLayout {
    private static final String TAG = "MessageContainer";

    interface MessageContainerA11yDelegate {
        void onA11yFocused();

        void onA11yFocusCleared();

        void onA11yDismiss();
    }

    class MessageContainerA11yDelegateProxy extends AccessibilityDelegate {
        private int mFocusedView;

        @Override
        public void onInitializeAccessibilityEvent(
                @NonNull View host, @NonNull AccessibilityEvent event) {
            handleEvent(event);
            super.onInitializeAccessibilityEvent(host, event);
        }

        @Override
        public boolean onRequestSendAccessibilityEvent(
                @NonNull ViewGroup host, @NonNull View child, @NonNull AccessibilityEvent event) {
            handleEvent(event);
            return super.onRequestSendAccessibilityEvent(host, child, event);
        }

        private void handleEvent(@NonNull AccessibilityEvent event) {
            if (mA11yDelegate == null) return;
            if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                assert mFocusedView == 0 : "No other view should be focused";
                mFocusedView++;
                mA11yDelegate.onA11yFocused();
            } else if (event.getEventType()
                    == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
                assert mFocusedView == 1 : "One view must be focused";
                mFocusedView--;
                mA11yDelegate.onA11yFocusCleared();
            }
        }
    }

    private MessageContainerA11yDelegate mA11yDelegate;
    private boolean mIsInitializingLayout;
    private int mA11yDismissActionId = NO_ID;

    public MessageContainer(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setAccessibilityDelegate(new MessageContainerA11yDelegateProxy());
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
        if (getChildCount() > 1) {
            throw new IllegalStateException(
                    "Should not contain more than 2 views when adding a new message.");
        } else if (getChildCount() == 1) {
            View cur = getChildAt(0);
            index = cur.getElevation() > view.getElevation() ? 1 : 0;
        }
        super.addView(view, index);
        onChildCountChanged();

        // TODO(crbug.com/40749472): clipChildren should be set to false only when the message is in
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
        onChildCountChanged();
    }

    private void onChildCountChanged() {
        ViewCompat.removeAccessibilityAction(this, mA11yDismissActionId);
        if (getChildCount() == 0) return;
        String label =
                getResources()
                        .getString(
                                getChildCount() == 1
                                        ? R.string.chrome_dismiss
                                        : R.string.message_dismiss_and_show_next);
        mA11yDismissActionId =
                ViewCompat.addAccessibilityAction(
                        this,
                        label,
                        (v, c) -> {
                            if (mA11yDelegate != null) {
                                assert getChildCount() != 0;
                                mA11yDelegate.onA11yDismiss();
                                return true;
                            }
                            return false;
                        });
    }

    public int getMessageBannerHeight() {
        assert getChildCount() > 0;
        // TODO(crbug.com/40877229): remove this log after fix.
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
     *
     * @param runnable The {@link Runnable}.
     * @return True if the callback is triggered immediately (i.e. synchronously).
     */
    boolean runAfterInitialMessageLayout(Runnable runnable) {
        View view = getChildAt(0);
        assert view != null;
        if (view.getHeight() > 0) {
            mIsInitializingLayout = false;
            runnable.run();
            return true;
        }

        mIsInitializingLayout = true;
        view.addOnLayoutChangeListener(
                new OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        if (v.getHeight() == 0) return;

                        runnable.run();
                        v.removeOnLayoutChangeListener(this);
                        mIsInitializingLayout = false;
                    }
                });
        return false;
    }

    /**
     * Returns whether container is initializing its layout for a new added view. Clients should not
     * call {@link #runAfterInitialMessageLayout(Runnable)} when it returns true.
     * @return True if it is initializing layout.
     */
    public boolean isIsInitializingLayout() {
        return mIsInitializingLayout;
    }

    /** Call {@link #addMessage(View)} instead in order to prevent from uncontrolled add. */
    @Override
    @Deprecated
    public final void addView(View view) {
        throw new RuntimeException("Use addMessage instead.");
    }

    /** Call {@link #removeMessage(View)} instead in order to prevent from uncontrolled remove. */
    @Override
    @Deprecated
    public final void removeView(View view) {
        throw new RuntimeException("Use removeMessage instead.");
    }

    public int getA11yDismissActionIdForTesting() {
        return mA11yDismissActionId;
    }
}
