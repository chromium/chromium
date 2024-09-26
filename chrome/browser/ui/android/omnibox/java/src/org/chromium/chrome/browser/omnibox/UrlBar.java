// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.Layout;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.ReplacementSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.accessibility.AccessibilityEvent;
import android.view.autofill.AutofillManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;
import androidx.core.text.TextDirectionHeuristicsCompat;
import androidx.core.view.inputmethod.EditorInfoCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.util.FirstDrawDetector;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;

/** The URL text entry view for the Omnibox. */
public class UrlBar extends AutocompleteEditText {
    private static final String TAG = "UrlBar";
    @VisibleForTesting static final float LINE_HEIGHT_FACTOR = 1.15f;

    private static final boolean DEBUG = false;

    // TextView becomes very slow on long strings, so we limit maximum length
    // of what is displayed to the user, see limitDisplayableLength().
    private static final int MAX_DISPLAYABLE_LENGTH = 4000;
    private static final int MAX_DISPLAYABLE_LENGTH_LOW_END = 1000;

    // Stylus handwriting: Setting this ime option instructs stylus writing service to restrict
    // capturing writing events slightly outside the Url bar area. This is needed to prevent stylus
    // handwriting in inputs in web content area that are very close to url bar area, from being
    // committed to Url bar's Edit text. Ex: google.com search field.
    private static final String IME_OPTION_RESTRICT_STYLUS_WRITING_AREA =
            "restrictDirectWritingArea=true";

    // The text must be at least this long to be truncated. Safety measure to prevent accidentally
    // over truncating text for large tablets and external displays. Also, tests can continue to
    // check for text equality, instead of worrying about partial equality with truncated text.
    static final int MIN_LENGTH_FOR_TRUNCATION = 100;

    /**
     * The text direction of the URL or query: LAYOUT_DIRECTION_LOCALE, LAYOUT_DIRECTION_LTR, or
     * LAYOUT_DIRECTION_RTL.
     */
    private int mUrlDirection;

    private UrlBarDelegate mUrlBarDelegate;
    private Optional<Callback<String>> mTextChangeListener;
    private @NonNull Optional<Runnable> mTypingStartedListener = Optional.empty();
    private Optional<OnKeyListener> mKeyDownListener;
    private UrlBarTextContextMenuDelegate mTextContextMenuDelegate;
    private Callback<Integer> mUrlDirectionListener;

    /**
     * The gesture detector is used to detect long presses. Long presses require special treatment
     * because the URL bar has custom touch event handling. See: {@link #onTouchEvent}.
     */
    private final KeyboardHideHelper mKeyboardHideHelper;

    private final Rect mClipBounds = new Rect();
    @VisibleForTesting final Runnable mEnforceMaxTextHeight = this::enforceMaxTextHeight;

    private boolean mFocused;
    private boolean mFocusEventEmitted;
    private boolean mAllowFocus = true;
    private boolean mTypingStartedEventSent;

    private boolean mPendingScroll;

    // Captures the current intended text scroll type.
    // This may not be effective if mPendingScroll is true.
    @ScrollType private int mCurrentScrollType;
    // Captures previously calculated text scroll type.
    @ScrollType private int mPreviousScrollType;
    private String mPreviousScrollText;
    private int mPreviousScrollViewWidth;
    private int mPreviousScrollResultXPosition;
    private int mPreviousScrollOriginEndIndex;
    private float mPreviousScrollFontSize;
    private boolean mPreviousScrollWasRtl;
    private CharSequence mVisibleTextPrefixHint;

    // Used as a hint to indicate the text may contain an ellipsize span.  This will be true if an
    // ellipsize span was applied the last time the text changed. A true value here does not
    // guarantee that the text does contain the span currently as newly set text may have cleared
    // this (and it the value will only be recalculated after the text has been changed).
    private boolean mDidEllipsizeTextHint;

    /**
     * The character index in the displayed text where the origin ends. This is required to ensure
     * that the end of the origin is not scrolled out of view for long hostnames.
     */
    private int mOriginEndIndex;

    /** What scrolling action should be taken after the URL bar text changes. * */
    @IntDef({ScrollType.NO_SCROLL, ScrollType.SCROLL_TO_TLD, ScrollType.SCROLL_TO_BEGINNING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollType {
        int NO_SCROLL = 0;
        int SCROLL_TO_TLD = 1;
        int SCROLL_TO_BEGINNING = 2;
    }

    /**
     * An optional string to use with AccessibilityNodeInfo to report text content. This is
     * particularly important for auto-fill applications, such as password managers, that rely on
     * AccessibilityNodeInfo data to apply related form-fill data.
     */
    private CharSequence mTextForAutofillServices;

    protected boolean mRequestingAutofillStructure;

    /** Delegate used to communicate with the content side and the parent layout. */
    public interface UrlBarDelegate {
        /**
         * @return The view to be focused on a backward focus traversal.
         */
        @Nullable
        View getViewForUrlBackFocus();

        /**
         * @return Whether the keyboard should be allowed to learn from the user input.
         */
        boolean allowKeyboardLearning();

        /** Called to notify that back key has been pressed while the URL bar has focus. */
        void backKeyPressed();

        /** Called to notify that UrlBar has been focused by touch. */
        void onFocusByTouch();

        /** Called to notify that UrlBar has been touched after focus. */
        void onTouchAfterFocus();
    }

    /** Delegate that provides the additional functionality to the textual context menus. */
    interface UrlBarTextContextMenuDelegate {
        /**
         * @return The text to be pasted into the UrlBar.
         */
        @NonNull
        String getTextToPaste();

        /**
         * Gets potential replacement text to be used instead of the current selected text for
         * cut/copy actions. If null is returned, the existing text will be cut or copied.
         *
         * @param currentText The current displayed text.
         * @param selectionStart The selection start in the display text.
         * @param selectionEnd The selection end in the display text.
         * @return The text to be cut/copied instead of the currently selected text.
         */
        @Nullable
        String getReplacementCutCopyText(String currentText, int selectionStart, int selectionEnd);
    }

    public UrlBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mUrlDirection = LAYOUT_DIRECTION_LOCALE;

        // The URL Bar is derived from an text edit class, and as such is focusable by
        // default. This means that if it is created before the first draw of the UI it
        // will (as the only focusable element of the UI) get focus on the first draw.
        // We react to this by greying out the tab area and bringing up the keyboard,
        // which we don't want to do at startup. Prevent this by disabling focus until
        // the first draw.
        setFocusable(false);
        setFocusableInTouchMode(false);
        setHorizontalFadingEdgeEnabled(true);
        // Disable elegant text height for now. We calculate font size at runtime, and try to
        // respect the user's need to increase the font size.
        // Enabling elegant text for UrlBar will likely produce smaller font when users ask for a
        // larger one.
        setElegantTextHeight(OmniboxFeatures.sElegantTextHeight.isEnabled());
        // Use a global draw instead of View#onDraw in case this View is not visible.
        FirstDrawDetector.waitForFirstDraw(
                this,
                () -> {
                    // We have now avoided the first draw problem (see the comments above) so we
                    // want to
                    // make the URL bar focusable so that touches etc. activate it.
                    setFocusable(mAllowFocus);
                    setFocusableInTouchMode(mAllowFocus);
                });

        setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        int verticalPadding =
                getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_padding);
        setPaddingRelative(0, verticalPadding, 0, verticalPadding);

        mKeyboardHideHelper =
                new KeyboardHideHelper(
                        this,
                        () -> {
                            if (mUrlBarDelegate != null && !BackPressManager.isEnabled()) {
                                mUrlBarDelegate.backKeyPressed();
                            }
                        });

        setTextClassifier(TextClassifier.NO_OP);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            setIsHandwritingDelegate(true);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.save();

        // Ensure glitch text does not render outside of the url bar bounds.
        // Glitch text can be generated online using glitch text generators.
        // Set the clipping bounds to the padding
        mClipBounds.left = getScrollX();
        mClipBounds.top = getPaddingTop();
        mClipBounds.right = getScrollX() + getWidth();
        mClipBounds.bottom = getHeight() - getPaddingBottom();
        canvas.clipRect(mClipBounds);

        super.onDraw(canvas);
        canvas.restore();

        // Notify listeners if the URL's direction has changed.
        updateUrlDirection();
    }

    public void destroy() {
        setAllowFocus(false);
        mUrlBarDelegate = null;
        setOnFocusChangeListener(null);
        mTextContextMenuDelegate = null;
        mTextChangeListener = Optional.empty();
        mTypingStartedListener = Optional.empty();
    }

    /** Initialize the delegate that allows interaction with the Window. */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mKeyboardHideHelper.setWindowDelegate(windowDelegate);
    }

    /** Set the delegate to be used for text context menu actions. */
    public void setTextContextMenuDelegate(UrlBarTextContextMenuDelegate delegate) {
        mTextContextMenuDelegate = delegate;
    }

    /**
     * When predictive back gesture is enabled, keycode_back will not be sent from Android OS
     * starting from T. {@link LocationBarMediator} will intercept the back press instead.
     */
    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (KeyEvent.KEYCODE_BACK == keyCode && event.getAction() == KeyEvent.ACTION_UP) {
            mKeyboardHideHelper.monitorForKeyboardHidden();
        }

        // NOTE: Do not pass ENTER key to listeners from here. This is because Enter key may also
        // come from a software keyboard.
        // - If we pass the event here, it will be emitted twice (once before IME and once after),
        // - if we don't pass the event after IME, soft keyboard navigation will not work.
        return (KeyNavigationUtil.isActionDown(event)
                        && !KeyNavigationUtil.isEnter(event)
                        && mKeyDownListener.map(l -> l.onKey(this, keyCode, event)).orElse(false))
                || super_onKeyPreIme(keyCode, event);
    }

    @CheckDiscard("exposed for testing; should be inlined")
    @VisibleForTesting
    public boolean super_onKeyPreIme(int keyCode, KeyEvent event) {
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return (KeyNavigationUtil.isEnter(event)
                        && mKeyDownListener.map(l -> l.onKey(this, keyCode, event)).orElse(false))
                || super_onKeyDown(keyCode, event);
    }

    @CheckDiscard("exposed for testing; should be inlined")
    @VisibleForTesting
    public boolean super_onKeyDown(int keyCode, KeyEvent event) {
        return super.onKeyDown(keyCode, event);
    }

    /**
     * See {@link AutocompleteEditText#setIgnoreTextChangesForAutocomplete(boolean)}.
     *
     * <p>{@link #setDelegate(UrlBarDelegate)} must be called with a non-null instance prior to
     * enabling autocomplete.
     */
    @Override
    public void setIgnoreTextChangesForAutocomplete(boolean ignoreAutocomplete) {
        super.setIgnoreTextChangesForAutocomplete(ignoreAutocomplete);
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        mTypingStartedEventSent = false;
        mFocused = focused;
        if (!mFocused) mFocusEventEmitted = false;
        super.onFocusChanged(focused, direction, previouslyFocusedRect);

        setHorizontalFadingEdgeEnabled(!focused);

        if (focused) {
            mPendingScroll = false;
        }
        fixupTextDirection();
    }

    @Override
    protected float getRightFadingEdgeStrength() {
        return 0.0f;
    }

    @Override
    protected float getLeftFadingEdgeStrength() {
        return getScrollX() > 0 ? 1.0f : 0.0f;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        setPrivateImeOptions(IME_OPTION_RESTRICT_STYLUS_WRITING_AREA);
    }

    /** Sets whether this {@link UrlBar} should be focusable. */
    public void setAllowFocus(boolean allowFocus) {
        mAllowFocus = allowFocus;
        setFocusable(allowFocus);
        setFocusableInTouchMode(allowFocus);
    }

    /**
     * Sends an accessibility event to the URL bar to request accessibility focus on it (e.g. for
     * TalkBack).
     */
    public void requestAccessibilityFocus() {
        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /**
     * Sets the {@link UrlBar}'s text direction based on focus and contents.
     *
     * <p>Should be called whenever focus or text contents change.
     */
    private void fixupTextDirection() {
        // When unfocused, force left-to-right rendering at the paragraph level (which is desired
        // for URLs). Right-to-left runs are still rendered RTL, but will not flip the whole URL
        // around. This is consistent with OmniboxViewViews on desktop. When focused, render text
        // normally (to allow users to make non-URL searches and to avoid showing Android's split
        // insertion point when an RTL user enters RTL text). Also render text normally when the
        // text field is empty (because then it displays an instruction that is not a URL).
        if (mFocused || length() == 0) {
            setTextDirection(TEXT_DIRECTION_INHERIT);
        } else {
            setTextDirection(TEXT_DIRECTION_LTR);
        }
        // Always align to the same as the paragraph direction (LTR = left, RTL = right).
        setTextAlignment(TEXT_ALIGNMENT_TEXT_START);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (DEBUG) Log.i(TAG, "onWindowFocusChanged: " + hasWindowFocus);
        if (hasWindowFocus) {
            if (isFocused()) {
                // Without the call to post(..), the keyboard was not getting shown when the
                // window regained focus despite this being the final call in the view system
                // flow.
                post(
                        new Runnable() {
                            @Override
                            public void run() {
                                KeyboardVisibilityDelegate.getInstance().showKeyboard(UrlBar.this);
                            }
                        });
            }
        }
    }

    @Override
    public View focusSearch(int direction) {
        if (mUrlBarDelegate != null
                && direction == View.FOCUS_BACKWARD
                && mUrlBarDelegate.getViewForUrlBackFocus() != null) {
            return mUrlBarDelegate.getViewForUrlBackFocus();
        } else {
            return super.focusSearch(direction);
        }
    }

    @Override
    protected void onTextChanged(CharSequence text, int start, int lengthBefore, int lengthAfter) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter);

        if (!mTypingStartedEventSent && mFocused && lengthAfter > 0) {
            mTypingStartedListener.ifPresent(Runnable::run);
            mTypingStartedEventSent = true;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Due to crbug.com/1103555, Autofill had to be disabled on the UrlBar to work around
            // an issue on Android Q+. With Autofill disabled, the Autofill compat mode no longer
            // learns of changes to the UrlBar, which prevents it from cancelling the session if
            // the domain changes. We restore this behavior by mimicking the relevant part of
            // TextView.notifyListeningManagersAfterTextChanged().
            // https://cs.android.com/android/platform/superproject/+/5d123b67756dffcfdebdb936ab2de2b29c799321:frameworks/base/core/java/android/widget/TextView.java;l=10618;drc=master;bpv=0
            final AutofillManager afm = getContext().getSystemService(AutofillManager.class);
            if (afm != null) {
                afm.notifyValueChanged(this);
            }
        }

        limitDisplayableLength();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP) {
            performClick();
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean performClick() {
        boolean result = super.performClick();
        if (mUrlBarDelegate == null) return result;

        // Don't emit subsequent events if we already notified the Delegate about how the Omnibox
        // was activated.
        if (mFocusEventEmitted) return result;
        mFocusEventEmitted = true;

        if (!mFocused) {
            // The Omnibox was inactive. This is the activation event.
            mUrlBarDelegate.onFocusByTouch();
        } else {
            // The Omnibox was was activated programmatically (e.g. on LFF devices with hardware
            // keyboard attached). Now, the user has explicitly clicked/touched the UrlBar.
            mUrlBarDelegate.onTouchAfterFocus();
        }

        return result;
    }

    /**
     * If the direction of the URL has changed, update mUrlDirection and notify the
     * UrlDirectionListeners.
     */
    private void updateUrlDirection() {
        Layout layout = getLayout();
        if (layout == null) return;

        int urlDirection;
        if (length() == 0) {
            urlDirection = LAYOUT_DIRECTION_LOCALE;
        } else if (layout.getParagraphDirection(0) == Layout.DIR_LEFT_TO_RIGHT) {
            urlDirection = LAYOUT_DIRECTION_LTR;
        } else {
            urlDirection = LAYOUT_DIRECTION_RTL;
        }

        if (urlDirection != mUrlDirection) {
            mUrlDirection = urlDirection;
            if (mUrlDirectionListener != null) {
                mUrlDirectionListener.onResult(urlDirection);
            }

            // Ensure the display text is visible after updating the URL direction.
            scrollDisplayText(mCurrentScrollType);
        }
    }

    /**
     * @return The text direction of the URL, e.g. LAYOUT_DIRECTION_LTR.
     */
    public int getUrlDirection() {
        return mUrlDirection;
    }

    /**
     * Sets the listener for changes in the url bar's layout direction. Also calls
     * onUrlDirectionChanged() immediately on the listener.
     *
     * @param listener The UrlDirectionListener to receive callbacks when the url direction changes,
     *     or null to unregister any previously registered listener.
     */
    public void setUrlDirectionListener(Callback<Integer> listener) {
        mUrlDirectionListener = listener;
        if (mUrlDirectionListener != null) {
            mUrlDirectionListener.onResult(mUrlDirection);
        }
    }

    /**
     * Set the url delegate to handle communication from the {@link UrlBar} to the rest of the UI.
     *
     * @param delegate The {@link UrlBarDelegate} to be used.
     */
    public void setDelegate(UrlBarDelegate delegate) {
        mUrlBarDelegate = delegate;
    }

    /**
     * Set the listener to be notified when the URL text has changed.
     *
     * @param listener The listener to be notified.
     */
    public void setTextChangeListener(Callback<String> listener) {
        mTextChangeListener = Optional.ofNullable(listener);
    }

    /**
     * Install the listener notified when the user begins typing in recently focused Omnibox for the
     * first time. When <null>, callback is removed.
     */
    /* package */ void setTypingStartedListener(@Nullable Runnable r) {
        mTypingStartedListener = Optional.ofNullable(r);
    }

    /**
     * Set the listener to be notified on each UrlBar KeyEvent.
     *
     * @param listener The listener to be notified.
     */
    public void setKeyDownListener(OnKeyListener listener) {
        mKeyDownListener = Optional.ofNullable(listener);
    }

    /** Set the text to report to Autofill services upon call to onProvideAutofillStructure. */
    public void setTextForAutofillServices(CharSequence text) {
        mTextForAutofillServices = text;
    }

    @Override
    public boolean onTextContextMenuItem(int id) {
        if (mTextContextMenuDelegate == null) return super.onTextContextMenuItem(id);

        if (id == android.R.id.paste) {
            String pasteString = mTextContextMenuDelegate.getTextToPaste();
            if (pasteString != null) {
                int min = 0;
                int max = getText().length();

                if (isFocused()) {
                    final int selStart = getSelectionStart();
                    final int selEnd = getSelectionEnd();

                    min = Math.max(0, Math.min(selStart, selEnd));
                    max = Math.max(0, Math.max(selStart, selEnd));
                }

                Selection.setSelection(getText(), max);
                getText().replace(min, max, pasteString);
                onPaste();
            }
            return true;
        }

        if ((id == android.R.id.cut || id == android.R.id.copy)) {
            if (id == android.R.id.cut) {
                RecordUserAction.record("Omnibox.LongPress.Cut");
            } else {
                RecordUserAction.record("Omnibox.LongPress.Copy");
            }
            String currentText = getText().toString();
            String replacementCutCopyText =
                    mTextContextMenuDelegate.getReplacementCutCopyText(
                            currentText, getSelectionStart(), getSelectionEnd());
            if (replacementCutCopyText == null) return super.onTextContextMenuItem(id);

            setIgnoreTextChangesForAutocomplete(true);
            setText(replacementCutCopyText);
            setSelection(0, replacementCutCopyText.length());
            setIgnoreTextChangesForAutocomplete(false);

            boolean retVal = super.onTextContextMenuItem(id);

            if (TextUtils.equals(getText(), replacementCutCopyText)) {
                // Restore the old text if the operation did modify the text.
                setIgnoreTextChangesForAutocomplete(true);
                setText(currentText);

                // Move the cursor to the end.
                setSelection(getText().length());
                setIgnoreTextChangesForAutocomplete(false);
            }

            return retVal;
        }

        if (id == android.R.id.shareText) {
            RecordUserAction.record("Omnibox.LongPress.Share");
            ShareHelper.recordShareSource(ShareHelper.ShareSourceAndroid.ANDROID_SHARE_SHEET);
        }

        return super.onTextContextMenuItem(id);
    }

    /**
     * Estimates how many characters fit in the viewport and truncates {@link text} before calling
     * setText({@link text}.
     *
     * @param text The text to set.
     * @param scrollType What type of scroll should be applied to the text.
     * @param scrollToIndex The index that should be scrolled to, which only applies to {@link
     *     ScrollType#SCROLL_TO_TLD}.
     */
    public void setTextWithTruncation(
            CharSequence text, @ScrollType int scrollType, int scrollToIndex) {
        if (mFocused
                || TextUtils.isEmpty(text)
                || text.length() < MIN_LENGTH_FOR_TRUNCATION
                || getLayoutParams().width == LayoutParams.WRAP_CONTENT
                || containsRtl(text)) {
            setText(text);
            return;
        }

        // Find the width of the url bar in device independent pixels (dp), then guess how many
        // characters are able to fit.
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(getContext());

        // Conservatively use the width/height of the entire screen while estimating how many
        // characters to truncate, to avoid dealing with changes in window size and orientation.
        int maxScreenDimension = Math.max(display.getDisplayHeight(), display.getDisplayWidth());
        int dp = DisplayUtil.pxToDp(display, maxScreenDimension);

        // Conservately estimates each char is 0.8mm on average.
        // 1 dp = 1/160 inches = ~0.158mm, so 5 dp = ~0.8mm.
        // This is a very rough estimate, chosen arbitrarily. The goal is not to truncate the url
        // so that it fits exactly in the url bar, but rather to truncate extremely long
        // (thousands of characters) urls down to something much shorter (tens or 100 characters).
        int truncationIndex = dp / 5;

        // We don't want to remove any part of the TLD. But, if we think that the TLD can fill up
        // the url bar, then we can truncate everything after the TLD, since nothing past the end of
        // the TLD is visible after scrolling.
        if (scrollType == ScrollType.SCROLL_TO_TLD) {
            truncationIndex = Math.max(scrollToIndex, truncationIndex);
        }

        truncationIndex = Math.min(text.length(), truncationIndex);
        CharSequence truncatedText = text.subSequence(0, truncationIndex);
        setText(truncatedText);
    }

    /**
     * Specified how text should be scrolled within the UrlBar.
     *
     * @param scrollType What type of scroll should be applied to the text.
     * @param scrollToIndex The index that should be scrolled to, which only applies to {@link
     *     ScrollType#SCROLL_TO_TLD}.
     */
    public void setScrollState(@ScrollType int scrollType, int scrollToIndex) {
        if (scrollType == ScrollType.SCROLL_TO_TLD) {
            mOriginEndIndex = scrollToIndex;
        } else {
            mOriginEndIndex = 0;
        }
        scrollDisplayText(scrollType);
    }

    /**
     * Return a hint of the currently visible text displayed on screen.
     *
     * <p>The hint represents the substring of the full text from the first character to the last
     * visible character displayed on screen. Depending on the length of this prefix, not all of the
     * characters will e displayed on screen.
     *
     * <p>This will null if:
     *
     * <ul>
     *   <li>The width constraints have changed since the hint was calculated.
     *   <li>The hint could not be correctly calculated.
     *   <li>The visible text is narrower than the viewport.
     * </ul>
     */
    public @Nullable CharSequence getVisibleTextPrefixHint() {
        if (getVisibleMeasuredViewportWidth() != mPreviousScrollViewWidth) return null;
        return mVisibleTextPrefixHint;
    }

    private int getVisibleMeasuredViewportWidth() {
        return getMeasuredWidth() - (getPaddingLeft() + getPaddingRight());
    }

    private boolean isVisibleTextTheSame(Editable text) {
        if (text == null) {
            return false;
        }

        if (mVisibleTextPrefixHint != null) {
            return TextUtils.indexOf(text, mVisibleTextPrefixHint) == 0;
        }

        return TextUtils.equals(text, mPreviousScrollText);
    }

    /**
     * Scrolls the omnibox text to the position specified, based on the {@link ScrollType}.
     *
     * @param scrollType What type of scroll to perform. SCROLL_TO_TLD: Scrolls the omnibox text to
     *     bring the TLD into view. SCROLL_TO_BEGINNING: Scrolls text that's too long to fit in the
     *     omnibox to the beginning so we can see the first character.
     */
    @VisibleForTesting
    public void scrollDisplayText(@ScrollType int scrollType) {
        // It's possible that text layout is not available right now. This could happen when the
        // call is made before the text could be measured. Fall back to safe defaults, even if not
        // correct for RTL layouts - this should be very rare (~10 cases per day worldwide).
        // The layout will likely be available after the view measures itself again.
        // Postpone scroll until we view and text layout are known.
        // Request scroll update in case scroll type or view dimensions have changed.
        mCurrentScrollType = scrollType;
        mPendingScroll = isLayoutRequested() || (getLayout() == null);
        if (mPendingScroll) return;

        if (mFocused) return;

        Editable text = getText();
        if (TextUtils.isEmpty(text)) scrollType = ScrollType.SCROLL_TO_BEGINNING;

        // Ensure any selection from the focus state is cleared.
        setSelection(0);

        float currentTextSize = getTextSize();
        boolean currentIsRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        int measuredWidth = getVisibleMeasuredViewportWidth();

        if (scrollType == mPreviousScrollType
                && measuredWidth == mPreviousScrollViewWidth
                // Font size is float but it changes in discrete range (eg small font, big font),
                // therefore false negative using regular equality is unlikely.
                && currentTextSize == mPreviousScrollFontSize
                && currentIsRtl == mPreviousScrollWasRtl
                && isVisibleTextTheSame(text)) {
            scrollTo(mPreviousScrollResultXPosition, getScrollY());

            return;
        }

        switch (scrollType) {
            case ScrollType.SCROLL_TO_TLD:
                final long startTime = SystemClock.elapsedRealtime();
                scrollToTLD();
                RecordHistogram.recordTimesHistogram(
                        "Omnibox.ScrollToTLD.Duration", SystemClock.elapsedRealtime() - startTime);
                break;
            case ScrollType.SCROLL_TO_BEGINNING:
                scrollToBeginning();
                break;
            default:
                // Intentional return to avoid clearing scroll state when no scroll was applied.
                return;
        }

        mPreviousScrollType = scrollType;
        mPreviousScrollText = text.toString();
        mPreviousScrollViewWidth = measuredWidth;
        mPreviousScrollFontSize = currentTextSize;
        mPreviousScrollResultXPosition = getScrollX();
        mPreviousScrollWasRtl = currentIsRtl;
        mPreviousScrollOriginEndIndex = mOriginEndIndex;
    }

    /** Scrolls the omnibox text to show the very beginning of the text entered. */
    @VisibleForTesting
    /* package */ void scrollToBeginning() {
        // Clear the visible text hint as this path is not used for normal browser navigation.
        // If that changes in the future, update this to actually calculate the visible text hints.
        mVisibleTextPrefixHint = null;

        Editable text = getText();
        float scrollPos = 0f;
        if (TextUtils.isEmpty(text)) {
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL
                    && BidiFormatter.getInstance().isRtl(getHint())) {
                // Compared to below that uses getPrimaryHorizontal(1) due to 0 returning an
                // invalid value, if the text is empty, getPrimaryHorizontal(0) returns the actual
                // max scroll amount.
                scrollPos = (int) getLayout().getPrimaryHorizontal(0) - getMeasuredWidth();
            }
        } else if (BidiFormatter.getInstance().isRtl(text)) {
            // RTL.
            float endPointX = getLayout().getPrimaryHorizontal(text.length());
            int measuredWidth = getMeasuredWidth();
            float width = getLayout().getPaint().measureText(text.toString());
            scrollPos = Math.max(0, endPointX - measuredWidth + width);
        }
        scrollTo((int) scrollPos, getScrollY());
    }

    /**
     * The visible hint contains the visible portion of the text in the url bar. It is used to
     * reduce toolbar captures. For example, in the case of same document navigations, some prefix
     * of the text will remain unchanged. If the url bar can't display more characters than this
     * prefix, then the visible hint will remain the same, and we might not have to do another
     * capture.
     *
     * @return A prefix of getText(), up to and including the last visible character.
     */
    @VisibleForTesting
    CharSequence calculateVisibleHint() {
        try (TimingMetric t = TimingMetric.shortUptime("Omnibox.CalculateVisibleHint.Duration")) {
            Editable url = getText();
            int measuredWidth = getVisibleMeasuredViewportWidth();
            int urlTextLength = url.length();

            Layout textLayout = getLayout();

            int finalVisibleCharIndex =
                    textLayout
                            .getPaint()
                            .getOffsetForAdvance(
                                    url, 0, urlTextLength, 0, urlTextLength, false, measuredWidth);

            RecordHistogram.recordCount1000Histogram(
                    "Omnibox.NumberOfVisibleCharacters", finalVisibleCharIndex);

            int finalVisibleCharIndexExclusive = Math.min(finalVisibleCharIndex + 1, urlTextLength);
            boolean visibleUrlContainsRtl =
                    containsRtl(url.subSequence(0, finalVisibleCharIndexExclusive));
            if (visibleUrlContainsRtl) {
                // getOffsetForAdvance does not calculate the correct index if there is RTL
                // text before finalVisibleCharIndex, so clear the visible text hint. If RTL
                // or Bi-Di URLs become more prevalant, update this to correctly calculate
                // the hint.
                return null;
            } else {
                if (BuildConfig.ENABLE_ASSERTS) {
                    float horizontal =
                            textLayout.getPrimaryHorizontal(finalVisibleCharIndexExclusive);
                    float width = (float) measuredWidth;

                    assert MathUtils.areFloatsEqual(horizontal, width) || horizontal > width
                            : "finalVisibleCharIndex is too small: "
                                    + String.valueOf(finalVisibleCharIndexExclusive)
                                    + ". If discovered locally please update crbug.com/1465967 with"
                                    + " the url.";
                }

                // To avoid issues where a small portion of the character following
                // finalVisibleCharIndex is visible on screen, be more conservative and
                // extend the visual hint by an additional character. In testing,
                // getOffsetForHorizontal returns the last fully visible character on
                // screen. By extending the offset by an additional character, the risk is
                // of having visual artifacts from the subsequence character on screen is
                // mitigated.
                return url.subSequence(0, finalVisibleCharIndexExclusive);
            }
        }
    }

    /** Scrolls the omnibox text to bring the TLD into view. */
    @VisibleForTesting
    /* package */ void scrollToTLD() {
        Editable url = getText();
        int measuredWidth = getVisibleMeasuredViewportWidth();
        int urlTextLength = url.length();

        Layout textLayout = getLayout();
        assert getLayout().getLineCount() == 1;
        final int originEndIndex = Math.min(mOriginEndIndex, urlTextLength);
        if (mOriginEndIndex > urlTextLength) {
            // If discovered locally, please update crbug.com/859219 with the steps to reproduce.
            assert false
                    : "Attempting to scroll past the end of the URL: "
                            + url
                            + ", end index: "
                            + mOriginEndIndex;
        }

        float endPointX = textLayout.getPrimaryHorizontal(originEndIndex);
        // Compare the position offset of the last character and the character prior to determine
        // the LTR-ness of the final component of the URL.
        float priorToEndPointX =
                urlTextLength == 1
                        ? 0
                        : textLayout.getPrimaryHorizontal(Math.max(0, originEndIndex - 1));

        float scrollPos;
        if (priorToEndPointX < endPointX) {
            // LTR
            scrollPos = Math.max(0, endPointX - measuredWidth);
            if (endPointX > measuredWidth) {
                // To avoid issues where a small portion of the character following originEndIndex
                // is visible on screen, be more conservative and extend the visual hint by an
                // additional character (this was not reproducible locally at time of authoring, but
                // the complexities of font rendering / measurement suggest a conservative approach
                // at the start).
                //
                // While this approach uses an additional character to ensure a conservative and
                // more reliable hint signal, this could also use pixel based padding to get the
                // visible character XX pixels past the end of the viewport. Potentially, utilizing
                // both the additional character and pixel based padding and using the more
                // conservative of the two could be done if this current approach is not always
                // reliable.
                mVisibleTextPrefixHint =
                        url.subSequence(0, Math.min(originEndIndex + 1, urlTextLength));
            } else {
                if (textLayout.getPrimaryHorizontal(urlTextLength) <= measuredWidth) {
                    // Only store the visibility hint if the text is wider than the viewport. Text
                    // narrower than the viewport is not a useful hint because a consumer would not
                    // understand if a subsequent character would be visible on screen or not.
                    //
                    // If issues arise where text that is very close to the visible viewport is
                    // causing issues with the reliability of visible hint, consider checking that
                    // the measured text is greater than the measured width plus a small additional
                    // padding.
                    mVisibleTextPrefixHint = null;
                } else {
                    if (ChromeFeatureList.sNoVisibleHintForDifferentTLD.isEnabled()) {
                        // TODO(b/357649034): revisit and simplify the logic, seek to obsolete
                        // mPreviousScrollOriginEndIndex if possible.
                        String previousTLD =
                                mPreviousScrollText == null
                                                || (mPreviousScrollText.length()
                                                        < mPreviousScrollOriginEndIndex)
                                        ? null
                                        : mPreviousScrollText.substring(
                                                0, mPreviousScrollOriginEndIndex);
                        if (!TextUtils.isEmpty(previousTLD)
                                && TextUtils.indexOf(url, previousTLD) == 0) {
                            mVisibleTextPrefixHint = calculateVisibleHint();
                        } else {
                            mVisibleTextPrefixHint = null;
                        }
                    } else {
                        mVisibleTextPrefixHint = calculateVisibleHint();
                    }
                }
            }
        } else {
            // RTL
            // Clear the visible text hint due to the complexities of Bi-Di text handling. If
            // RTL or Bi-Di URLs become more prevalant, update this to correctly calculate the
            // hint.
            mVisibleTextPrefixHint = null;

            // To handle BiDirectional text, search backward from the two existing offsets to find
            // the first LTR character.  Ensure the final RTL component of the domain is visible
            // above any of the prior LTR pieces.
            int rtlStartIndexForEndingRun = originEndIndex - 1;
            for (int i = originEndIndex - 2; i >= 0; i--) {
                float indexOffsetDrawPosition = textLayout.getPrimaryHorizontal(i);
                if (indexOffsetDrawPosition > endPointX) {
                    rtlStartIndexForEndingRun = i;
                } else {
                    // getPrimaryHorizontal determines the index position for the next character
                    // based on the previous characters.  In bi-directional text, the first RTL
                    // character following LTR text will have an LTR-appearing horizontal offset
                    // as it is based on the preceding LTR text.  Thus, the start of the RTL
                    // character run will be after and including the first LTR horizontal index.
                    rtlStartIndexForEndingRun = Math.max(0, rtlStartIndexForEndingRun - 1);
                    break;
                }
            }
            float width =
                    textLayout
                            .getPaint()
                            .measureText(
                                    url.subSequence(rtlStartIndexForEndingRun, originEndIndex)
                                            .toString());
            if (width < measuredWidth) {
                scrollPos = Math.max(0, endPointX + width - measuredWidth);
            } else {
                scrollPos = endPointX + measuredWidth;
            }
        }
        scrollTo((int) scrollPos, getScrollY());
    }

    @Override
    public void layout(int left, int top, int right, int bottom) {
        super.layout(left, top, right, bottom);
        // Do not scale the Omnibox font size if our height is set to WRAP_CONTENT.
        // This ensures we don't trigger the recurring layout/adjust/layout/adjust cycle.
        if (getLayoutParams().height != LayoutParams.WRAP_CONTENT) {
            post(mEnforceMaxTextHeight);
        }

        // Note: this must happen after the *entire* layout cycle completes.
        // Running this during onLayout guarantees that isLayoutRequested will remain true,
        // and the text layout will remain unresolved, suppressing resolution of display text
        // scroll position.
        if (mPendingScroll || mPreviousScrollViewWidth != getVisibleMeasuredViewportWidth()) {
            scrollDisplayText(mCurrentScrollType);
            // Confirmation check: be sure we don't re-request layout as a result of something that
            // happens in scrollDisplayText().
            assert !isLayoutRequested();
        }
    }

    @Override
    public boolean bringPointIntoView(int offset) {
        // TextView internally attempts to keep the selection visible, but in the unfocused state
        // this class ensures that the TLD is visible.
        if (!mFocused) return false;
        assert !mPendingScroll;

        return super.bringPointIntoView(offset);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection connection = super.onCreateInputConnection(outAttrs);
        if (mUrlBarDelegate == null || !mUrlBarDelegate.allowKeyboardLearning()) {
            outAttrs.imeOptions |= EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING;
        }
        // Note: we apply this text variation here (as opposed to Constructor), because we want the
        // Editor to behave slightly differently than Keyboards:
        // - we want Editor to permit word selection on long-press, and
        // - we want Soft keyboards to stop auto-correcting user input.
        // This happens with certain modern soft keyboards, such as SwiftKey, that corrects spelling
        // of some urls (e.g. "flipkart.com" -> "flip cart. com" or "flipkart. com") despite
        // TYPE_TEXT_FLAG_NO_SUGGESTIONS and lack of TYPE_TEXT_FLAG_AUTO_CORRECT.
        outAttrs.inputType |= EditorInfo.TYPE_TEXT_VARIATION_URI;
        return connection;
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        if (DEBUG) Log.i(TAG, "setText -- text: %s", text);
        super.setText(text, type);

        fixupTextDirection();

        if (mVisibleTextPrefixHint != null
                && (text == null || TextUtils.indexOf(text, mVisibleTextPrefixHint) != 0)) {
            mVisibleTextPrefixHint = null;
        }
    }

    private void limitDisplayableLength() {
        // To limit displayable length we replace middle portion of the string with ellipsis.
        // That affects only presentation of the text, and doesn't affect other aspects like
        // copying to the clipboard, getting text with getText(), etc.
        final int maxLength =
                SysUtils.isLowEndDevice() ? MAX_DISPLAYABLE_LENGTH_LOW_END : MAX_DISPLAYABLE_LENGTH;

        Editable text = getText();
        int textLength = text.length();
        if (textLength <= maxLength) {
            if (mDidEllipsizeTextHint) {
                EllipsisSpan[] spans = text.getSpans(0, textLength, EllipsisSpan.class);
                if (spans != null && spans.length > 0) {
                    assert spans.length == 1 : "Should never apply more than a single EllipsisSpan";
                    for (int i = 0; i < spans.length; i++) {
                        text.removeSpan(spans[i]);
                    }
                }
            }
            mDidEllipsizeTextHint = false;
            return;
        }

        mDidEllipsizeTextHint = true;

        int spanLeft = text.nextSpanTransition(0, textLength, EllipsisSpan.class);
        if (spanLeft != textLength) return;

        spanLeft = maxLength / 2;
        text.setSpan(
                EllipsisSpan.INSTANCE,
                spanLeft,
                textLength - spanLeft,
                Editable.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    @Override
    public void requestLayout() {
        // TODO(crbug.com/40285597): it is speculated that a requestLayout invoked during an active
        // layout pass is causing Omnibox/Chrome to become unresponsive.
        // While Android seemingly supports that, emitting just a warning, we can't rule this out
        // completely. It is currently unclear where the secondary requestLayout could come from.
        if (isInLayout()) return;
        super.requestLayout();
    }

    @Override
    public Editable getText() {
        if (mRequestingAutofillStructure) {
            // crbug.com/1109186: mTextForAutofillServices must not be null here, but Autofill
            // requests can be triggered before it is initialized.
            return new SpannableStringBuilder(
                    mTextForAutofillServices != null ? mTextForAutofillServices : "");
        }
        return super.getText();
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        // When UrlBar is used as a read-only TextView, force Talkback to pronounce it like
        // TextView. Otherwise Talkback will say "Edit box, http://...". crbug.com/636988
        if (isEnabled()) {
            return super.getAccessibilityClassName();
        } else {
            return TextView.class.getName();
        }
    }

    @Override
    public void replaceAllTextFromAutocomplete(String text) {
        if (DEBUG) Log.i(TAG, "replaceAllTextFromAutocomplete: " + text);
        setText(text);
    }

    @Override
    public void onAutocompleteTextStateChanged(boolean updateDisplay) {
        mTextChangeListener.ifPresent(l -> l.onResult(getTextWithoutAutocomplete()));
    }

    private boolean containsRtl(CharSequence text) {
        BidiFormatter bidi =
                new BidiFormatter.Builder()
                        .setTextDirectionHeuristic(TextDirectionHeuristicsCompat.ANYRTL_LTR)
                        .build();
        return bidi.isRtl(text);
    }

    @VisibleForTesting
    void enforceMaxTextHeight() {
        int viewHeight = getHeight() - getPaddingTop() - getPaddingBottom();
        // Don't touch the text size if the view has not measured and shown yet, or if it's a
        // subject to custom layout constraints (e.g. CCT) that might result with font size being
        // too small.
        if (viewHeight <= 0) return;

        float effectiveFontHeightPx = getMaxHeightOfFont();

        if (getPaint().isElegantTextHeight()) {
            // http://go/ui-font-deprecation: when enabled, line height will be increased by up to
            // 60%.
            effectiveFontHeightPx *= getLineHeight() / getTextSize();
        } else {
            // Otherwise, scale the font down a little bit so it doesn't extend edge to edge.
            // This ensures we present the user with properly rendered UI and that we respect their
            // choice to use larger font (within the bounds permitted by url bar height).
            effectiveFontHeightPx *= LINE_HEIGHT_FACTOR;
        }

        if (effectiveFontHeightPx > viewHeight) {
            // we need to shrink the text to fit in the text field.
            var scaleRatio = viewHeight / effectiveFontHeightPx;
            setTextSize(TypedValue.COMPLEX_UNIT_PX, getTextSize() * scaleRatio);
        }
    }

    @VisibleForTesting
    @Px
    float getMaxHeightOfFont() {
        var fontMetrics = getPaint().getFontMetrics();
        return fontMetrics.bottom - fontMetrics.top;
    }

    /**
     * Span that displays ellipsis instead of the text. Used to hide portion of very large string to
     * get decent performance from TextView.
     */
    private static class EllipsisSpan extends ReplacementSpan {
        private static final String ELLIPSIS = "...";

        public static final EllipsisSpan INSTANCE = new EllipsisSpan();

        @Override
        public int getSize(
                Paint paint, CharSequence text, int start, int end, Paint.FontMetricsInt fm) {
            return (int) paint.measureText(ELLIPSIS);
        }

        @Override
        public void draw(
                Canvas canvas,
                CharSequence text,
                int start,
                int end,
                float x,
                int top,
                int y,
                int bottom,
                Paint paint) {
            canvas.drawText(ELLIPSIS, x, y, paint);
        }
    }

    /* package */ boolean hasPendingDisplayTextScrollForTesting() {
        return mPendingScroll;
    }
}
