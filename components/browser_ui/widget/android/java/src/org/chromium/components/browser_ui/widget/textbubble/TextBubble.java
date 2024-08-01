// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.textbubble;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.HashSet;
import java.util.Set;

/** UI component that handles showing a text callout bubble. */
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

    /** A supplier which notifies of changes of text bubbles count. */
    private static final ObservableSupplierImpl<Integer> sCountSupplier =
            new ObservableSupplierImpl<>();

    /** Disable assert error if it fails to be displayed. */
    private static boolean sSkipShowCheckForTesting;

    protected final Context mContext;
    private final Handler mHandler;
    private final boolean mInverseColor;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final AnchoredPopupWindow mPopupWindow;

    /** The {@link Drawable} that is responsible for drawing the bubble and the arrow. */
    @Nullable private ArrowBubbleDrawable mBubbleDrawable;

    /** The {@link Drawable} that precedes the text in the bubble. */
    protected final Drawable mImageDrawable;

    /** Runnables for snoozable text bubble option. */
    private final Runnable mSnoozeRunnable;

    private final Runnable mSnoozeDismissRunnable;

    /** Time tracking for histograms. */
    private long mBubbleShowStartTime;

    private final Runnable mDismissRunnable =
            new Runnable() {
                @Override
                public void run() {
                    if (mPopupWindow.isShowing()) dismiss();
                }
            };

    private final OnDismissListener mDismissListener =
            new OnDismissListener() {
                @Override
                public void onDismiss() {
                    sBubbles.remove(TextBubble.this);
                    sCountSupplier.set(sBubbles.size());
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

    private final boolean mIsAccessibilityEnabled;

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
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            View anchorView,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                stringId,
                accessibilityStringId,
                true,
                new ViewRectProvider(anchorView),
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance using the default arrow drawable background. Creates
     * a {@link RectProvider} using the provided {@code anchorRect}.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRect The {@link Rect} used to anchor the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            Rect anchorRect,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                stringId,
                accessibilityStringId,
                true,
                new RectProvider(anchorRect),
                isAccessibilityEnabled);
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
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            boolean showArrow,
            Rect anchorRect,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                stringId,
                accessibilityStringId,
                showArrow,
                new RectProvider(anchorRect),
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance using the default arrow drawable background.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            RectProvider anchorRectProvider,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                stringId,
                accessibilityStringId,
                true,
                anchorRectProvider,
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            boolean showArrow,
            RectProvider anchorRectProvider,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                context.getString(stringId),
                context.getString(accessibilityStringId),
                showArrow,
                anchorRectProvider,
                /* imageDrawable= */ null,
                /* isRoundBubble= */ false,
                /* inverseColor= */ false,
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance with no preceding image.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param contentString The string for the text that should be shown.
     * @param accessibilityString The string shown in the bubble when accessibility is enabled.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            String contentString,
            String accessibilityString,
            boolean showArrow,
            RectProvider anchorRectProvider,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                contentString,
                accessibilityString,
                showArrow,
                anchorRectProvider,
                /* imageDrawable= */ null,
                /* isRoundBubble= */ false,
                /* inverseColor= */ false,
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance with a preceding image.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param showArrow Whether the bubble should have an arrow. Should be false if {@code
     *         isRoundBubble} is true.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawableId The resource id of the image to show at the start of the text bubble.
     * @param isRoundBubble Whether the bubble should be round.
     * @param inverseColor Whether the background and icon/text colors should be inverted.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            boolean showArrow,
            RectProvider anchorRectProvider,
            @DrawableRes int imageDrawableId,
            boolean isRoundBubble,
            boolean inverseColor,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                context.getString(stringId),
                context.getString(accessibilityStringId),
                showArrow,
                anchorRectProvider,
                AppCompatResources.getDrawable(context, imageDrawableId),
                isRoundBubble,
                inverseColor,
                isAccessibilityEnabled);
    }

    /**
     * Constructs a {@link TextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param contentString The string for the text that should be shown.
     * @param accessibilityString The string shown in the bubble when accessibility is enabled.
     * @param showArrow Whether the bubble should have an arrow. Should be false if {@code
     *         isRoundBubble} is true.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawable The image to show at the start of the text bubble, or null if there
     *         should be no image.
     * @param isRoundBubble Whether the bubble should be round.
     * @param inverseColor Whether the background and icon/text colors should be inverted.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     */
    public TextBubble(
            Context context,
            View rootView,
            String contentString,
            String accessibilityString,
            boolean showArrow,
            RectProvider anchorRectProvider,
            @Nullable Drawable imageDrawable,
            boolean isRoundBubble,
            boolean inverseColor,
            boolean isAccessibilityEnabled) {
        this(
                context,
                rootView,
                contentString,
                accessibilityString,
                showArrow,
                anchorRectProvider,
                imageDrawable,
                isRoundBubble,
                inverseColor,
                isAccessibilityEnabled,
                null,
                null);
    }

    /**
     * Constructs a {@link TextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param contentString The string for the text that should be shown.
     * @param accessibilityString The string shown in the bubble when accessibility is enabled.
     * @param showArrow Whether the bubble should have an arrow. Should be false if {@code
     *         isRoundBubble} is true.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawable The image to show at the start of the text bubble, or null if there
     *         should be no image.
     * @param isRoundBubble Whether the bubble should be round.
     * @param inverseColor Whether the background and icon/text colors should be inverted.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *         text and dismiss UX.
     * At most one of the two following arguments will be non-null. Used in Snooze IPH experiment.
     * @param snoozeRunnable The callback for when snooze button is clicked.
     * @param snoozeDismissRunnable The callback to be invoked when dismiss button is clicked.
     */
    public TextBubble(
            Context context,
            View rootView,
            String contentString,
            String accessibilityString,
            boolean showArrow,
            RectProvider anchorRectProvider,
            @Nullable Drawable imageDrawable,
            boolean isRoundBubble,
            boolean inverseColor,
            boolean isAccessibilityEnabled,
            @Nullable Runnable snoozeRunnable,
            @Nullable Runnable snoozeDismissRunnable) {
        assert snoozeRunnable == null || snoozeDismissRunnable == null;
        mContext = context;
        mString = contentString;
        mAccessibilityString = accessibilityString;
        mImageDrawable = imageDrawable;
        mInverseColor = inverseColor;
        mIsAccessibilityEnabled = isAccessibilityEnabled;
        mSnoozeRunnable = snoozeRunnable;
        mSnoozeDismissRunnable = snoozeDismissRunnable;

        // For round, arrowless bubbles, we use a specialized background instead of the
        // ArrowBubbleDrawable.
        Drawable backgroundDrawable = getBackground(mContext, showArrow, isRoundBubble);

        mContentView = createContentView();
        // On some versions of Android, the LayoutParams aren't set until after the popup window
        // is shown. Explicitly set the LayoutParams to avoid crashing. See
        // https://crbug.com/713759.
        mContentView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        mPopupWindow =
                new AnchoredPopupWindow(
                        context, rootView, backgroundDrawable, mContentView, anchorRectProvider);
        mPopupWindow.setMargin(
                context.getResources().getDimensionPixelSize(R.dimen.text_bubble_margin));
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);
        mPopupWindow.setLayoutObserver(this);

        mHandler = new Handler();

        mPopupWindow.setAnimationStyle(R.style.TextBubbleAnimation);

        addOnDismissListener(mDismissListener);
        if (mIsAccessibilityEnabled) setDismissOnTouchInteraction(true);
    }

    /** Get the background to use. May be overridden by subclasses. */
    protected Drawable getBackground(Context context, boolean showArrow, boolean isRoundBubble) {
        mBubbleDrawable = new ArrowBubbleDrawable(mContext, isRoundBubble);
        mBubbleDrawable.setShowArrow(showArrow);
        // Set predefined styles for the TextBubble.
        if (mInverseColor) {
            mBubbleDrawable.setBubbleColor(SemanticColorUtils.getDefaultBgColor(mContext));
        } else {
            mBubbleDrawable.setBubbleColor(
                    SemanticColorUtils.getDefaultControlColorActive(mContext));
        }
        return mBubbleDrawable;
    }

    /** Shows the bubble. Will have no effect if the bubble is already showing. */
    public void show() {
        if (mPopupWindow.isShowing()) return;

        if (!mPopupWindow.isShowing() && mAutoDismissTimeoutMs != NO_TIMEOUT) {
            mHandler.postDelayed(mDismissRunnable, mAutoDismissTimeoutMs);
        }

        mPopupWindow.show();

        boolean popupShowing = sSkipShowCheckForTesting || mPopupWindow.isShowing();
        assert popupShowing : "TextBubble is not presented: " + mString;

        if (!popupShowing) return;

        sBubbles.add(this);
        sCountSupplier.set(sBubbles.size());
        mBubbleShowStartTime = System.currentTimeMillis();
    }

    /**
     * Disposes of the bubble.  Will have no effect if the bubble isn't showing.
     * @see PopupWindow#dismiss()
     */
    public void dismiss() {
        if (mPopupWindow.isShowing() && mBubbleShowStartTime != 0) {
            RecordHistogram.recordTimesHistogram(
                    "InProductHelp.TextBubble.ShownTime",
                    System.currentTimeMillis() - mBubbleShowStartTime);
            mBubbleShowStartTime = 0;
        }

        mPopupWindow.dismiss();
    }

    /** @return Whether the bubble is currently showing. */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /** Dismisses all the currently showing bubbles. */
    public static void dismissBubbles() {
        Set<TextBubble> bubbles = new HashSet<>(sBubbles);
        for (TextBubble bubble : bubbles) {
            bubble.dismiss();
        }
    }

    /**
     * @return A supplier which notifies of changes of text bubbles count.
     * */
    public static ObservableSupplier<Integer> getCountSupplier() {
        return sCountSupplier;
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
     * Changes the focusability of the bubble. If you are considering using this method, see
     * crbug.com/1240841 for additional context on UX considerations and possible risks in testing.
     * @param focusable True if the bubble is focusable, false otherwise.
     */
    public void setFocusable(boolean focusable) {
        mPopupWindow.setFocusable(focusable);
    }

    /**
     * Updates the timeout that is used to determine when to automatically dismiss the bubble.  If
     * the bubble is already showing, the timeout will start from the time of this call.  Any
     * previous timeouts will be canceled.  {@link #NO_TIMEOUT} is the default value.
     * @param timeoutMs The time (in milliseconds) the bubble should be dismissed after.  Use
     *                  {@link #NO_TIMEOUT} for no timeout.
     */
    public void setAutoDismissTimeout(long timeoutMs) {
        if (mIsAccessibilityEnabled) return;

        mAutoDismissTimeoutMs = timeoutMs;
        mHandler.removeCallbacks(mDismissRunnable);
        if (mPopupWindow.isShowing() && mAutoDismissTimeoutMs != NO_TIMEOUT) {
            mHandler.postDelayed(mDismissRunnable, mAutoDismissTimeoutMs);
        }
    }

    /**
     * @param dismiss Whether or not to dismiss this bubble when the screen is tapped.  This will
     *                happen for both taps inside and outside the popup except when a tap is handled
     *                by child views. The default is {@code false}.
     */
    public void setDismissOnTouchInteraction(boolean dismiss) {
        // For accessibility mode, since there is no timeout value, the bubble can be dismissed
        // only on touch interaction.
        mPopupWindow.setDismissOnTouchInteraction(mIsAccessibilityEnabled || dismiss);
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
        // mBubbleDrawable might not be in use if a subclass replaces the drawable.
        if (mBubbleDrawable == null) return;

        int arrowXOffset = 0;
        if (mBubbleDrawable.isShowingArrow()) {
            arrowXOffset = anchorRect.centerX() - x;

            // Force the anchor to be in a reasonable spot w.r.t. the bubble (not over the corners).
            int minArrowOffset = mBubbleDrawable.getArrowLeftSpacing();
            int maxArrowOffset = width - mBubbleDrawable.getArrowRightSpacing();
            arrowXOffset = MathUtils.clamp(arrowXOffset, minArrowOffset, maxArrowOffset);
        }

        // TODO(dtrainor): Figure out how to move the arrow and bubble to make things look
        // better.

        mBubbleDrawable.setPositionProperties(arrowXOffset, positionBelow);
    }

    /** @return The content view to show in the TextBubble. */
    private View createContentView() {
        if (mImageDrawable == null) {
            View view = LayoutInflater.from(mContext).inflate(R.layout.textbubble_text, null);
            setText(view.findViewById(R.id.message));

            // Set different paddings for when snooze feature is present.
            if (mSnoozeRunnable != null || mSnoozeDismissRunnable != null) {
                int paddingStart =
                        mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.text_bubble_with_snooze_padding_horizontal);
                int paddingEnd =
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.text_bubble_with_snooze_padding_end);
                TextView text = view.findViewById(R.id.message);
                text.setPadding(
                        paddingStart, text.getPaddingTop(), paddingEnd, text.getPaddingBottom());
            }

            if (mSnoozeRunnable != null) {
                Button snoozeButton = (Button) view.findViewById(R.id.button_snooze);
                snoozeButton.setVisibility(View.VISIBLE);
                snoozeButton.setOnClickListener(
                        v -> {
                            mSnoozeRunnable.run();
                            mDismissRunnable.run();
                        });
            } else if (mSnoozeDismissRunnable != null) {
                Button dismissButton = (Button) view.findViewById(R.id.button_dismiss);
                dismissButton.setVisibility(View.VISIBLE);
                dismissButton.setOnClickListener(
                        v -> {
                            mSnoozeDismissRunnable.run();
                            mDismissRunnable.run();
                        });
            }
            return view;
        }
        View view =
                LayoutInflater.from(mContext).inflate(R.layout.textbubble_text_with_image, null);
        ImageView imageView = view.findViewById(R.id.image);
        imageView.setImageDrawable(mImageDrawable);
        if (mInverseColor) {
            imageView.setColorFilter(SemanticColorUtils.getDefaultControlColorActive(mContext));
        }
        setText(view.findViewById(R.id.message));
        return view;
    }

    /** @param view The {@link TextView} to set text on. */
    private void setText(TextView view) {
        view.setText(mIsAccessibilityEnabled ? mAccessibilityString : mString);
        updateTextStyle(view, mInverseColor);
    }

    /**
     * @param isInverse Whether the color scheme is inversed or not.
     * @param view The {@link TextView} to update the style for.
     */
    protected void updateTextStyle(TextView view, boolean isInverse) {
        if (isInverse) {
            view.setTextAppearance(R.style.TextAppearance_TextMediumThick_Accent1);
        }
    }

    /** For testing only, get the list of active text bubbles. */
    public static Set<TextBubble> getTextBubbleSetForTesting() {
        return sBubbles;
    }

    /** For testing only, get the content view of a TextBubble. */
    public View getTextBubbleContentViewForTesting() {
        return mContentView;
    }

    public static void setSkipShowCheckForTesting(boolean skip) {
        sSkipShowCheckForTesting = skip;
    }
}
