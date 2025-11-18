// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView.OnEditorActionListener;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Manages the UI component for the one-line(inline) search box used in the History page's normal
 * view.
 *
 * <p>This class encapsulates the layout inflation, view management (EditText, clear button), and
 * user interaction logic (text changes, focus handling, clear button clicks) for the search UI.
 */
@NullMarked
public class InlineSearchBox {

    private LinearLayout mInlineSearchView;

    private EditText mInlineSearchEditText;

    private ImageButton mInlineClearTextButton;

    private ViewGroup mInlineSearchBoxContainer;

    private final SearchDelegate mSearchDelegate;

    private final ObservableSupplierImpl<Boolean> mHasSearchTextSupplier;

    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    public InlineSearchBox(
            SearchDelegate searchDelegate,
            ObservableSupplierImpl<Boolean> hasSearchTextSupplier,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate) {
        mSearchDelegate = searchDelegate;
        mHasSearchTextSupplier = hasSearchTextSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
    }

    /**
     * Inflates and initializes the search box container and its child views. This includes finding
     * all child views, setting listeners, and setting the background.
     *
     * @param parent The parent view to inflate the layout into.
     * @param hintStringResId The resource ID for the search box's hint text.
     * @param onEditorActionListener The listener for editor actions (e.g., search).
     * @param context The Context used for layout inflation.
     */
    @Initializer
    public void initializeSearchBoxContainer(
            @Nullable ViewGroup parent,
            @StringRes int hintStringResId,
            OnEditorActionListener onEditorActionListener,
            Context context) {

        mInlineSearchBoxContainer =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.search_toolbar, parent, false);

        mInlineSearchView = mInlineSearchBoxContainer.findViewById(R.id.search_view);
        mInlineSearchView.setVisibility(View.VISIBLE);
        mInlineSearchEditText = mInlineSearchView.findViewById(R.id.search_text);
        mInlineClearTextButton = mInlineSearchView.findViewById(R.id.clear_text_button);

        initializeSearchText(hintStringResId, onEditorActionListener);
        initializeClearTextButton();

        mInlineSearchBoxContainer.setBackgroundResource(R.drawable.search_toolbar_modern_bg);
    }

    /**
     * Initializes the inline search text view ({@code mInlineSearchEditText}) and its listeners.
     *
     * @param hintStringResId The resource ID of the hint string to display in the search box.
     * @param onEditorActionListener The {@link OnEditorActionListener} to respond to keyboard
     *     actions.
     */
    private void initializeSearchText(
            @StringRes int hintStringResId, OnEditorActionListener onEditorActionListener) {
        mInlineSearchEditText.setHint(hintStringResId);
        mInlineSearchEditText.setOnEditorActionListener(onEditorActionListener);
        mInlineSearchEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        mInlineClearTextButton.setVisibility(
                                TextUtils.isEmpty(s) ? View.INVISIBLE : View.VISIBLE);
                        mHasSearchTextSupplier.set(!TextUtils.isEmpty(s));
                        mSearchDelegate.onSearchTextChanged(s.toString());
                    }
                });
        mInlineSearchEditText.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        v.requestFocus();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                });
        requestSearchFocus(true);
    }

    /**
     * Sets the OnClickListener for the inline clear text button. When clicked, the button will call
     * {@link #clearSearch()}.
     */
    private void initializeClearTextButton() {
        mInlineClearTextButton.setOnClickListener(
                v -> {
                    clearSearch();
                });
    }

    /**
     * In SelectableListToolbar, the padding/margin of the original search box is dynamically
     * calculated(in onDisplayStyleChanged()), so dynamical adjustment is needed for the inline
     * search box too.
     */
    public void setInlinePadding(int left, int top, int right, int bottom) {
        if (mInlineSearchBoxContainer == null) return;
        mInlineSearchBoxContainer.post(
                () -> {
                    int[] appLocation = new int[2];
                    int[] location = new int[2];
                    View contentView =
                            mInlineSearchBoxContainer
                                    .getRootView()
                                    .findViewById(android.R.id.content);
                    if (contentView == null) return;
                    contentView.getLocationOnScreen(appLocation);
                    mInlineSearchBoxContainer.getLocationOnScreen(location);
                    mInlineSearchBoxContainer.setPaddingRelative(
                            left - (location[0] - appLocation[0]), top, right, bottom);
                });
    }

    /**
     * Requests focus for the search EditText.
     *
     * @param showKeyboard If true, also attempts to show the soft keyboard.
     */
    public void requestSearchFocus(boolean showKeyboard) {
        mInlineSearchEditText.post(
                () -> {
                    mInlineSearchEditText.requestFocus();
                    if (showKeyboard) {
                        mKeyboardVisibilityDelegate.showKeyboard(mInlineSearchEditText);
                    }
                });
    }

    public void clearSearch() {
        mInlineSearchEditText.setText("");
        mInlineSearchEditText.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    public boolean hasSearchText() {
        if (mInlineSearchEditText == null) return false;
        return !TextUtils.isEmpty(mInlineSearchEditText.getText());
    }

    public EditText getSearchText() {
        return mInlineSearchEditText;
    }

    public ViewGroup getSearchBoxContainer() {
        return mInlineSearchBoxContainer;
    }
}
