// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/**
 * A view that contains the omnibox suggestions dropdown. This view is responsible for measuring and
 * positioning the dropdown.
 */
@NullMarked
public class OmniboxSuggestionsContainer extends FrameLayout {
    private OmniboxSuggestionsDropdown mDropdown;
    private @Nullable OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private @Nullable Callback<Integer> mHeightChangeListener;
    private OmniboxAlignment mOmniboxAlignment = OmniboxAlignment.UNSPECIFIED;

    private int mListViewMaxHeight;
    private int mLastBroadcastedListViewMaxHeight;
    private final Callback<OmniboxAlignment> mOmniboxAlignmentObserver =
            this::onOmniboxAlignmentChanged;

    public OmniboxSuggestionsContainer(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDropdown = findViewById(R.id.omnibox_suggestions_dropdown);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        boolean isTablet = mEmbedder != null && mEmbedder.isTablet();

        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Measure");
                TimingMetric metric = OmniboxMetrics.recordSuggestionListMeasureTime();
                TimingMetric metric2 = OmniboxMetrics.recordSuggestionListMeasureWallTime()) {
            maybeUpdateLayoutParams(mOmniboxAlignment.top);
            int availableViewportHeight = mOmniboxAlignment.height;
            int desiredWidth = mOmniboxAlignment.width;
            adjustHorizontalPosition();
            notifyObserversIfViewportHeightChanged(availableViewportHeight);

            widthMeasureSpec = MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY);
            heightMeasureSpec =
                    MeasureSpec.makeMeasureSpec(
                            availableViewportHeight,
                            isTablet ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            if (isTablet) {
                setRoundBottomCorners(
                        getMeasuredHeight() < availableViewportHeight
                                || !KeyboardVisibilityDelegate.getInstance()
                                        .isKeyboardShowing(this));
            }
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return mDropdown.onKeyDown(keyCode, event) || super.onKeyDown(keyCode, event);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        // Propagate touch events, to make possible touch elements behind this container. Omnibox
        // autofocus feature prevents the Scrim to be shown as a result tab content is covered by
        // this transparent container.
        boolean shouldPassThroughUnhandledTouchEvents =
                mEmbedder != null && mEmbedder.shouldPassThroughUnhandledTouchEvents();
        if (shouldPassThroughUnhandledTouchEvents) {
            return false;
        }

        // Swallow all touch events, especially if these were not consumed by the Dropdown.
        // This ensures that touching the blank areas of the container does not dismiss the
        // Omnibox.
        // Making the container `clickable=true` achieves similar goal, but this consumes all
        // activators, including keyboard <Enter> key.
        super.onTouchEvent(event);
        return true;
    }

    private void maybeUpdateLayoutParams(int topMargin) {
        // Update the layout params to ensure the parent correctly positions the suggestions
        // under the anchor view.
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        if (layoutParams != null && layoutParams instanceof ViewGroup.MarginLayoutParams) {
            ((ViewGroup.MarginLayoutParams) layoutParams).topMargin = topMargin;
        }
    }

    private void notifyObserversIfViewportHeightChanged(int availableViewportHeight) {
        if (availableViewportHeight == mListViewMaxHeight) return;

        mListViewMaxHeight = availableViewportHeight;
        if (mHeightChangeListener != null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Detect if there was another change since this task posted.
                        // This indicates a subsequent task being posted too.
                        if (mListViewMaxHeight != availableViewportHeight) return;
                        // Detect if the new height is the same as previously broadcasted.
                        // The two checks (one above and one below) allow us to detect quick
                        // A->B->A transitions and suppress the broadcasts.
                        if (mLastBroadcastedListViewMaxHeight == availableViewportHeight) return;
                        if (mHeightChangeListener == null) return;

                        mHeightChangeListener.onResult(availableViewportHeight);
                        mLastBroadcastedListViewMaxHeight = availableViewportHeight;
                    });
        }
    }

    private void adjustHorizontalPosition() {
        // Set our left edge using translation x. This avoids needing to relayout (like setting
        // a left margin would) and is less risky than calling View#setLeft(), which is intended
        // for use by the layout system.
        setTranslationX(mOmniboxAlignment.left);
    }

    private void setRoundBottomCorners(boolean roundBottomCorners) {
        ViewOutlineProvider outlineProvider = getOutlineProvider();
        if (outlineProvider instanceof RoundedCornerOutlineProvider roundedCornerOutlineProvider) {
            roundedCornerOutlineProvider.setRoundingEdges(true, true, true, roundBottomCorners);
        }
    }

    /**
     * Sets the embedder for the list view.
     *
     * @param embedder the embedder of this list.
     */
    public void setEmbedder(OmniboxSuggestionsDropdownEmbedder embedder) {
        // Don't reset the current value of `mOmniboxAlignment`, and don't read the value from newly
        // installed embedder to ensure the `onOmniboxAlignmentChanged` does the right thing when we
        // install our observers.
        mEmbedder = embedder;
    }

    /**
     * Respond to Omnibox session state change.
     *
     * @param urlHasFocus whether URL has focus (signaling the session is active)
     */
    /* package */ void onOmniboxSessionStateChange(boolean urlHasFocus) {
        if (urlHasFocus) {
            installAlignmentObserver();
        } else {
            mDropdown.cancelWindowContentChangedAnnouncement();
            removeAlignmentObserver();
        }
    }

    private void installAlignmentObserver() {
        if (mEmbedder != null) {
            mEmbedder.onAttachedToWindow();
            mOmniboxAlignment = mEmbedder.addAlignmentObserver(mOmniboxAlignmentObserver);
        }
    }

    private void removeAlignmentObserver() {
        if (mEmbedder != null) {
            mEmbedder.onDetachedFromWindow();
            mEmbedder.removeAlignmentObserver(mOmniboxAlignmentObserver);
        }

        if (!OmniboxFeatures.shouldPreWarmRecyclerViewPool()) {
            mDropdown.getRecycledViewPool().clear();
        }
    }

    private void onOmniboxAlignmentChanged(OmniboxAlignment omniboxAlignment) {
        boolean isOnlyHorizontalDifference =
                omniboxAlignment.isOnlyHorizontalDifference(mOmniboxAlignment);
        boolean isWidthDifference = omniboxAlignment.doesWidthDiffer(mOmniboxAlignment);
        mOmniboxAlignment = omniboxAlignment;
        mDropdown.setPaddingRelative(
                mDropdown.getPaddingStart(),
                mDropdown.getPaddingTop(),
                mDropdown.getPaddingEnd(),
                mDropdown.getBaseBottomPadding() + mOmniboxAlignment.paddingBottom);

        if (isOnlyHorizontalDifference) {
            adjustHorizontalPosition();
            return;
        } else if (isWidthDifference) {
            // If our width has changed, we may have views in our pool that are now the wrong width.
            // Recycle them by calling swapAdapter() to avoid showing views of the wrong size.
            mDropdown.swapAdapter(mDropdown.getAdapter(), true);
            Configuration configuration = getContext().getResources().getConfiguration();
            mDropdown.setClipToOutline(
                    configuration.screenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);
            BaseSuggestionViewBinder.resetCachedResources();
        }
        if (isInLayout()) {
            // requestLayout doesn't behave predictably in the middle of a layout pass. Even if it
            // does trigger a second layout pass, measurement caches aren't properly reset,
            // resulting in stale sizing. Absent a way to abort the current pass and start over the
            // simplest solution is to wait until the current pass is over to request relayout.
            PostTask.postTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> {
                        ViewUtils.requestLayout(
                                OmniboxSuggestionsContainer.this,
                                "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
                    });
        } else {
            ViewUtils.requestLayout(
                    (View) OmniboxSuggestionsContainer.this,
                    "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
        }
    }

    /**
     * Sets the listener for changes of the suggestion list's height. The height may change as a
     * result of eg. soft keyboard popping up.
     *
     * @param listener A listener will receive the new height of the suggestion list in pixels.
     */
    public void setHeightChangeListener(Callback<Integer> listener) {
        mHeightChangeListener = listener;
    }

    /** Clean up resources and remove observers installed by this class. */
    public void destroy() {
        mDropdown.destroy();
        mHeightChangeListener = null;
    }

    @VisibleForTesting
    void setSuggestionsDropdownForTest(OmniboxSuggestionsDropdown dropdown) {
        mDropdown = dropdown;
    }
}
