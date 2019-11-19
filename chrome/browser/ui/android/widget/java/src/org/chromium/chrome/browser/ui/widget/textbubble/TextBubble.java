// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.textbubble;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ui.widget.R;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.HashSet;
import java.util.Set;

/**
 * UI component that handles showing a text callout bubble.
 */
public class TextBubble implements AnchoredPopupWindow.LayoutObserver {
    /**
     * Specifies no limit to the popup duration.
     * @see #setAutoDismissTimeout(long)
     */
    public static final long NO_TIMEOUT = 0;

    /**
     * A set of bubbles which are active at this moment. This set can be used to dismiss the
     * bubbles on a back press event.
     */
    private static final Set<TextBubble> sBubbles = new HashSet<>();

    protected final Context mContext;
    private final Handler mHandler;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final AnchoredPopupWindow mPopupWindow;

    /** The {@link Drawable} that is responsible for drawing the bubble and the arrow. */
    private final ArrowBubbleDrawable mDrawable;

    private final Runnable mDismissRunnable = new Runnable() {
        @Override
        public void run() {
            if (mPopupWindow.isShowing()) dismiss();
        }
    };

    private final OnDismissListener mDismissListener = new OnDismissListener() {
        @Override
        public void onDismiss() {
            sBubbles.remove(TextBubble.this);
        }
    };

    /**
     * How long to wait before automatically dismissing the bubble.  {@link #NO_TIMEOUT} is the
     * default and means the bubble will stay visible indefinitely.
     */
    private long mAutoDismissTimeoutMs = NO_TIMEOUT;

    // Content specific variables.
    /** The string to show in the bubble. */
    private final String mString;

    /** The accessibility string associated with the bubble. */
    private final String mAccessibilityString;

    /** The content view shown in the popup window. */
    protected View mContentView;

    /**
     * Constructs a {@link TextBubble} instance using the default arrow drawable background. Creates
     * a {@link ViewRectProvider} using the provided {@code anchorView}.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorView The {@link View} used to anchor the bubble.
     */
    public TextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, View anchorView) {
        this(context, rootView, stringId, accessibilityStringId, true,
                new ViewRectProvider(anchorView));
    }

    /**
     * Constructs a {@link TextBubble} instance using the default arrow drawable background. Creates
     * a {@link RectProvider} using the provided {@code anchorRect}.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRect The {@link Rect} used to anchor the text bubble.
     */
    public TextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, Rect anchorRect) {
        this(context, rootView, stringId, accessibilityStringId, true,
                new RectProvider(anchorRect));
    }

    /**
     * Constructs a {@link TextBubble} instance. Creates a {@link RectProvider} using the provided
     * {@code anchorRect}.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRect The {@link Rect} used to anchor the text bubble.
     */
    public TextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, boolean showArrow, Rect anchorRect) {
        this(context, rootView, stringId, accessibilityStringId, showArrow,
                new RectProvider(anchorRect));
    }

    /**
     * Constructs a {@link TextBubble} instance using the default arrow drawable background.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     */
    public TextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, RectProvider anchorRectProvider) {
        this(context, rootView, stringId, accessibilityStringId, true, anchorRectProvider);
    }

    /**
     * Constructs a {@link TextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     */
    public TextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, boolean showArrow,
            RectProvider anchorRectProvider) {
        this(context, rootView, context.getString(stringId),
                context.getString(accessibilityStringId), showArrow, anchorRectProvider);
    }

    /**
     * Constructs a {@link TextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param contentString The string for the text that should be shown.
     * @param accessibilityString The string shown in the bubble when accessibility is enabled.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     */
    public TextBubble(Context context, View rootView, String contentString,
            String accessibilityString, boolean showArrow, RectProvider anchorRectProvider) {
        mContext = context;
        mString = contentString;
        mAccessibilityString = accessibilityString;

        mDrawable = new ArrowBubbleDrawable(context);
        mDrawable.setShowArrow(showArrow);

        mContentView = createContentView();
        // On some versions of Android, the LayoutParams aren't set until after the popup window
        // is shown. Explicitly set the LayoutParams to avoid crashing. See
        // https://crbug.com/713759.
        mContentView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        mPopupWindow = new AnchoredPopupWindow(
                context, rootView, mDrawable, mContentView, anchorRectProvider);
        mPopupWindow.setMargin(
                context.getResources().getDimensionPixelSize(R.dimen.text_bubble_margin));
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);
        mPopupWindow.setLayoutObserver(this);

        mHandler = new Handler();

        mPopupWindow.setAnimationStyle(R.style.TextBubbleAnimation);

        addOnDismissListener(mDismissListener);
        if (AccessibilityUtil.isAccessibilityEnabled()) setDismissOnTouchInteraction(true);

        // Set predefined styles for the TextBubble.
        mDrawable.setBubbleColor(ApiCompatibilityUtils.getColor(
                mContext.getResources(), R.color.light_active_color));
    }

    /** Shows the bubble. Will have no effect if the bubble is already showing. */
    public void show() {
        if (mPopupWindow.isShowing()) return;

        if (!mPopupWindow.isShowing() && mAutoDismissTimeoutMs != NO_TIMEOUT) {
            mHandler.postDelayed(mDismissRunnable, mAutoDismissTimeoutMs);
        }

        mPopupWindow.show();
        sBubbles.add(this);
    }

    /**
     * Disposes of the bubble.  Will have no effect if the bubble isn't showing.
     * @see PopupWindow#dismiss()
     */
    public void dismiss() {
        mPopupWindow.dismiss();
    }

    /**
     * @return Whether the bubble is currently showing.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /**
     * Dismisses all the currently showing bubbles.
     */
    public static void dismissBubbles() {
        Set<TextBubble> bubbles = new HashSet<>(sBubbles);
        for (TextBubble bubble : bubbles) {
            bubble.dismiss();
        }
    }

    /**
     * @param onTouchListener A callback for all touch events being dispatched to the bubble.
     * @see PopupWindow#setTouchInterceptor(OnTouchListener)
     */
    public void setTouchInterceptor(OnTouchListener onTouchListener) {
        mPopupWindow.setTouchInterceptor(onTouchListener);
    }

    /**
     * @param onDismissListener A listener to be called when the bubble is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void addOnDismissListener(OnDismissListener onDismissListener) {
        mPopupWindow.addOnDismissListener(onDismissListener);
    }

    /**
     * @param onDismissListener The listener to remove and not call when the bubble is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void removeOnDismissListener(OnDismissListener onDismissListener) {
        mPopupWindow.removeOnDismissListener(onDismissListener);
    }

    /**
     * Updates the timeout that is used to determine when to automatically dismiss the bubble.  If
     * the bubble is already showing, the timeout will start from the time of this call.  Any
     * previous timeouts will be canceled.  {@link #NO_TIMEOUT} is the default value.
     * @param timeoutMs The time (in milliseconds) the bubble should be dismissed after.  Use
     *                  {@link #NO_TIMEOUT} for no timeout.
     */
    public void setAutoDismissTimeout(long timeoutMs) {
        if (AccessibilityUtil.isAccessibilityEnabled()) return;

        mAutoDismissTimeoutMs = timeoutMs;
        mHandler.removeCallbacks(mDismissRunnable);
        if (mPopupWindow.isShowing() && mAutoDismissTimeoutMs != NO_TIMEOUT) {
            mHandler.postDelayed(mDismissRunnable, mAutoDismissTimeoutMs);
        }
    }

    /**
     * @param dismiss Whether or not to dismiss this bubble when the screen is tapped.  This will
     *                happen for both taps inside and outside the popup.  The default is
     *                {@code false}.
     */
    public void setDismissOnTouchInteraction(boolean dismiss) {
        // For accessibility mode, since there is no timeout value, the bubble can be dismissed
        // only on touch interaction.
        if (AccessibilityUtil.isAccessibilityEnabled()) dismiss = true;

        mPopupWindow.setDismissOnTouchInteraction(dismiss);
    }

    /**
     * Sets the preferred vertical orientation of the bubble with respect to the anchor view such as
     * above or below the anchor.
     * @param orientation The vertical orientation preferred.
     */
    public void setPreferredVerticalOrientation(
            @AnchoredPopupWindow.VerticalOrientation int orientation) {
        mPopupWindow.setPreferredVerticalOrientation(orientation);
    }

    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        int arrowXOffset = 0;
        if (mDrawable.isShowingArrow()) {
            arrowXOffset = anchorRect.centerX() - x;

            // Force the anchor to be in a reasonable spot w.r.t. the bubble (not over the corners).
            int minArrowOffset = mDrawable.getArrowLeftSpacing();
            int maxArrowOffset = width - mDrawable.getArrowRightSpacing();
            arrowXOffset = MathUtils.clamp(arrowXOffset, minArrowOffset, maxArrowOffset);
        }

        // TODO(dtrainor): Figure out how to move the arrow and bubble to make things look
        // better.

        mDrawable.setPositionProperties(arrowXOffset, positionBelow);
    }

    /**
     * @return The content view to show in the TextBubble.
     */
    protected View createContentView() {
        View view = LayoutInflater.from(mContext).inflate(R.layout.textbubble_text, null);
        setText((TextView) view);
        return view;
    }

    /**
     * @param view The {@link TextView} to set text on.
     */
    protected void setText(TextView view) {
        view.setText(AccessibilityUtil.isAccessibilityEnabled() ? mAccessibilityString : mString);
    }
}
