// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.MenuRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.Toolbar;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.NumberRollView;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * A toolbar that changes its view depending on whether a selection is established. The toolbar
 * also optionally shows a search view depending on whether {@link #initializeSearchView()} has
 * been called.
 *
 * @param <E> The type of the selectable items this toolbar interacts with.
 */
public class SelectableListToolbar<E> extends Toolbar
        implements SelectionObserver<E>,
                OnClickListener,
                OnEditorActionListener,
                DisplayStyleObserver {
    /** A delegate that handles searching the list of selectable items associated with this toolbar. */
    public interface SearchDelegate {
        /**
         * Called when the text in the search EditText box has changed.
         * @param query The text in the search EditText box.
         */
        void onSearchTextChanged(String query);

        /** Called when a search is ended. */
        void onEndSearch();
    }

    @IntDef({ViewType.NORMAL_VIEW, ViewType.SELECTION_VIEW, ViewType.SEARCH_VIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int NORMAL_VIEW = 0;
        int SELECTION_VIEW = 1;
        int SEARCH_VIEW = 2;
    }

    @IntDef({
        NavigationButton.NONE,
        NavigationButton.SEARCH_BACK,
        NavigationButton.SELECTION_BACK,
        NavigationButton.NORMAL_VIEW_BACK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationButton {
        int NONE = 0;
        int SEARCH_BACK = 1;
        int SELECTION_BACK = 2;
        int NORMAL_VIEW_BACK = 3;
    }

    protected boolean mIsSelectionEnabled;
    protected SelectionDelegate<E> mSelectionDelegate;

    private final ObservableSupplierImpl<Boolean> mIsSearchingSupplier =
            new ObservableSupplierImpl<>();
    private boolean mHasSearchView;
    private LinearLayout mSearchView;
    private EditText mSearchEditText;
    private ImageButton mClearTextButton;
    private SearchDelegate mSearchDelegate;
    private boolean mSearchEnabled;
    private boolean mUpdateStatusBarColor;
    private boolean mShowBackInNormalView;

    protected NumberRollView mNumberRollView;
    private Drawable mMenuButton;
    private Drawable mNavigationIconDrawable;

    private @NavigationButton int mNavigationButton;
    private @StringRes int mTitleResId;
    private @IdRes int mSearchMenuItemId;
    private @IdRes int mInfoMenuItemId;
    private @IdRes int mNormalGroupResId;
    private @IdRes int mSelectedGroupResId;

    private @ColorInt int mNormalBackgroundColor;
    private @ColorInt int mSearchBackgroundColor;
    private ColorStateList mIconColorList;

    private UiConfig mUiConfig;
    private int mWideDisplayStartOffsetPx;
    private int mModernNavButtonStartOffsetPx;
    private int mModernToolbarActionMenuEndOffsetPx;
    private int mModernToolbarSearchIconOffsetPx;

    private boolean mIsDestroyed;

    private boolean mShowInfoIcon;
    private int mShowInfoStringId;
    private int mHideInfoStringId;

    // current view type that SelectableListToolbar is showing
    private int mViewType;

    /** Constructor for inflating from XML. */
    public SelectableListToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsSearchingSupplier.set(false);
    }

    /** Destroys and cleans up itself. */
    @CallSuper
    public void destroy() {
        mIsDestroyed = true;
        if (mSelectionDelegate != null) mSelectionDelegate.removeObserver(this);
        if (mSearchEditText != null) hideKeyboard();
    }

    /**
     * @see {@link #initialize(SelectionDelegate, int, int, int, boolean, int, boolean)}.
     */
    public void initialize(
            SelectionDelegate<E> delegate,
            int titleResId,
            int normalGroupResId,
            int selectedGroupResId,
            boolean updateStatusBarColor) {
        initialize(
                delegate,
                titleResId,
                normalGroupResId,
                selectedGroupResId,
                updateStatusBarColor,
                /* menuResId= */ 0,
                /* showBackInNormalView= */ false);
    }

    /**
     * Initializes the SelectionToolbar.
     *
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *     set a title when the selection is cleared.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *     established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *     established.
     * @param updateStatusBarColor Whether the status bar color should be updated to match the
     *     toolbar color. If true, the status bar will only be updated if the current device fully
     *     supports theming and is on Android M+.
     * @param menuResId The resource id of the menu. {@code 0} if not required.
     * @param showBackInNormalView Whether the back button should be shown in normal view.
     */
    public void initialize(
            SelectionDelegate<E> delegate,
            @StringRes int titleResId,
            @IdRes int normalGroupResId,
            @IdRes int selectedGroupResId,
            boolean updateStatusBarColor,
            @MenuRes int menuResId,
            boolean showBackInNormalView) {
        mTitleResId = titleResId;
        if (menuResId != Resources.ID_NULL) inflateMenu(menuResId);
        mNormalGroupResId = normalGroupResId;
        mSelectedGroupResId = selectedGroupResId;
        // TODO(twellington): Setting the status bar color crashes on Nokia devices. Re-enable
        // after a Nokia test device is procured and the crash can be debugged.
        // See https://crbug.com/880694.
        mUpdateStatusBarColor = false;

        mSelectionDelegate = delegate;
        mSelectionDelegate.addObserver(this);

        mModernNavButtonStartOffsetPx =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.selectable_list_toolbar_nav_button_start_offset);
        mModernToolbarActionMenuEndOffsetPx =
                getResources()
                        .getDimensionPixelSize(R.dimen.selectable_list_action_bar_end_padding);
        mModernToolbarSearchIconOffsetPx =
                getResources()
                        .getDimensionPixelSize(R.dimen.selectable_list_search_icon_end_padding);

        mNormalBackgroundColor = SemanticColorUtils.getDefaultBgColor(getContext());
        setBackgroundColor(mNormalBackgroundColor);

        mIconColorList =
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_tint_list);

        setTitleTextAppearance(getContext(), R.style.TextAppearance_Headline_Primary);
        if (mTitleResId != 0) setTitle(mTitleResId);

        mMenuButton =
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.ic_more_vert_24dp,
                        R.color.default_icon_color_secondary_tint_list);
        setOverflowIcon(mMenuButton);
        mNavigationIconDrawable =
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.ic_arrow_back_white_24dp,
                        R.color.default_icon_color_tint_list);

        mShowInfoIcon = true;
        mShowInfoStringId = R.string.show_info;
        mHideInfoStringId = R.string.hide_info;

        if (showBackInNormalView) {
            mShowBackInNormalView = true;
            setNavigationButton(NavigationButton.NORMAL_VIEW_BACK);
        }
    }

    /**
     * Inflates and initializes the search view.
     *
     * @param searchDelegate The delegate that will handle performing searches.
     * @param hintStringResId The hint text to show in the search view's EditText box.
     * @param searchMenuItemId The menu item used to activate the search view. This item will be
     *     hidden when selection is enabled or if the list of selectable items associated with this
     *     toolbar is empty.
     */
    public void initializeSearchView(
            SearchDelegate searchDelegate,
            @StringRes int hintStringResId,
            @IdRes int searchMenuItemId) {
        mHasSearchView = true;
        mSearchDelegate = searchDelegate;
        mSearchMenuItemId = searchMenuItemId;
        mSearchBackgroundColor = Color.WHITE;

        LayoutInflater.from(getContext()).inflate(R.layout.search_toolbar, this);

        mSearchView = findViewById(R.id.search_view);
        mSearchEditText = mSearchView.findViewById(R.id.search_text);
        mSearchEditText.setHint(hintStringResId);
        mSearchEditText.setOnEditorActionListener(this);
        mSearchEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        mClearTextButton.setVisibility(
                                TextUtils.isEmpty(s) ? View.INVISIBLE : View.VISIBLE);
                        if (isSearching()) mSearchDelegate.onSearchTextChanged(s.toString());
                    }
                });

        mClearTextButton = findViewById(R.id.clear_text_button);
        mClearTextButton.setOnClickListener(v -> mSearchEditText.setText(""));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LayoutInflater.from(getContext()).inflate(R.layout.number_roll_view, this);
        mNumberRollView = findViewById(R.id.selection_mode_number);
        mNumberRollView.setString(R.plurals.selected_items);
        mNumberRollView.setStringForZero(R.string.select_items);
    }

    @Override
    @CallSuper
    public void onSelectionStateChange(List<E> selectedItems) {
        boolean wasSelectionEnabled = mIsSelectionEnabled;
        mIsSelectionEnabled = mSelectionDelegate.isSelectionEnabled();

        // If onSelectionStateChange() gets called before onFinishInflate(), mNumberRollView
        // will be uninitialized. See crbug.com/637948.
        if (mNumberRollView == null) {
            mNumberRollView = findViewById(R.id.selection_mode_number);
        }

        if (mIsSelectionEnabled) {
            showSelectionView(selectedItems, wasSelectionEnabled);
        } else if (isSearching()) {
            showSearchViewInternal();
        } else {
            showNormalView();
        }

        if (mIsSelectionEnabled) {
            @StringRes
            int resId =
                    wasSelectionEnabled
                            ? R.string.accessibility_toolbar_multi_select
                            : R.string.accessibility_toolbar_screen_position;
            announceForAccessibility(
                    getContext().getString(resId, Integer.toString(selectedItems.size())));
        }
    }

    @Override
    public void onClick(View view) {
        if (mIsDestroyed) return;

        switch (mNavigationButton) {
            case NavigationButton.NONE:
                break;
            case NavigationButton.SEARCH_BACK:
                onSearchNavigationBack();
                break;
            case NavigationButton.SELECTION_BACK:
                mSelectionDelegate.clearSelection();
                break;
            case NavigationButton.NORMAL_VIEW_BACK:
                onNavigationBack();
                break;
            default:
                assert false : "Incorrect navigation button state";
        }
    }

    /**
     * Handle a click on the search navigation back button. If this toolbar has a search view, the
     * search view will be hidden.
     */
    public void onSearchNavigationBack() {
        if (!mHasSearchView || !isSearching()) return;

        hideSearchView();
    }

    /**
     * Handle a click on the normal view back button. Can be overridden to give behavior to the back
     * button on the normal view.
     */
    protected void onNavigationBack() {}

    /**
     * Update the current navigation button (the top-left icon on LTR)
     *
     * @param navigationButton one of NAVIGATION_BUTTON_* constants.
     */
    protected void setNavigationButton(@NavigationButton int navigationButton) {
        @StringRes int contentDescriptionId = Resources.ID_NULL;

        mNavigationButton = navigationButton;
        setNavigationOnClickListener(this);

        switch (mNavigationButton) {
            case NavigationButton.NONE:
                break;
            case NavigationButton.SEARCH_BACK:
                DrawableCompat.setTintList(mNavigationIconDrawable, mIconColorList);
                contentDescriptionId = R.string.accessibility_toolbar_btn_back;
                break;
            case NavigationButton.SELECTION_BACK:
                DrawableCompat.setTintList(mNavigationIconDrawable, mIconColorList);
                contentDescriptionId = R.string.accessibility_cancel_selection;
                break;
            case NavigationButton.NORMAL_VIEW_BACK:
                DrawableCompat.setTintList(
                        mNavigationIconDrawable,
                        AppCompatResources.getColorStateList(
                                getContext(), R.color.default_icon_color_secondary_tint_list));
                contentDescriptionId = R.string.accessibility_toolbar_btn_back;
                break;
            default:
                assert false : "Incorrect navigationButton argument";
        }

        setNavigationIcon(
                contentDescriptionId == Resources.ID_NULL ? null : mNavigationIconDrawable);
        setNavigationContentDescription(contentDescriptionId);

        updateDisplayStyleIfNecessary();
    }

    /** Shows the search edit text box and related views. */
    public void showSearchView(boolean showKeyboard) {
        assert mHasSearchView;

        mIsSearchingSupplier.set(true);
        mSelectionDelegate.clearSelection();

        showSearchViewInternal();

        mSearchEditText.requestFocus();
        if (showKeyboard) {
            KeyboardVisibilityDelegate.getInstance().showKeyboard(mSearchEditText);
        }

        setTitle(null);
    }

    /** Hides the search edit text box and related views. Notifies delegate of the change. */
    public void hideSearchView() {
        hideSearchView(/* notifyDelegate= */ true);
    }

    /**
     * Hides the search edit text box and related views.
     * @param notifyDelegate Whether to notify the delegate of this change.
     */
    public void hideSearchView(boolean notifyDelegate) {
        assert mHasSearchView;

        if (!isSearching()) return;

        mIsSearchingSupplier.set(false);
        mSearchEditText.setText("");
        hideKeyboard();
        showNormalView();

        if (notifyDelegate) mSearchDelegate.onEndSearch();
    }

    /**
     * Called to enable/disable search menu button.
     * @param searchEnabled Whether the search button should be enabled.
     */
    public void setSearchEnabled(boolean searchEnabled) {
        if (mHasSearchView) {
            mSearchEnabled = searchEnabled;
            updateSearchMenuItem();
        }
    }

    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (actionId == EditorInfo.IME_ACTION_SEARCH) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(v);
        }
        return false;
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mIsDestroyed) return;

        if (mSelectionDelegate != null) mSelectionDelegate.clearSelection();
        if (isSearching()) hideSearchView();
    }

    /**
     * When the toolbar has a wide display style, its contents will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the toolbar contents will be visually centered by
     * adding padding to both sides.
     *
     * @param uiConfig The UiConfig used to observe display style changes.
     */
    public void configureWideDisplayStyle(UiConfig uiConfig) {
        mWideDisplayStartOffsetPx =
                getResources().getDimensionPixelSize(R.dimen.toolbar_wide_display_start_offset);

        mUiConfig = uiConfig;
        mUiConfig.addObserver(this);
    }

    @Override
    public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        int padding =
                SelectableListLayout.getPaddingForDisplayStyle(newDisplayStyle, getResources());
        int paddingStartOffset = 0;
        boolean isSearchViewShowing = isSearching() && !mIsSelectionEnabled;
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();

        if (newDisplayStyle.horizontal == HorizontalDisplayStyle.WIDE
                && !(isSearching()
                        || mIsSelectionEnabled
                        || mNavigationButton != NavigationButton.NONE)) {
            // The title in the wide display should be aligned with the texts of the list elements.
            paddingStartOffset = mWideDisplayStartOffsetPx;
        }

        // The margin instead of padding will be set to adjust the modern search view background
        // in search mode.
        if (newDisplayStyle.horizontal == HorizontalDisplayStyle.WIDE && isSearchViewShowing) {
            params.setMargins(padding, params.topMargin, padding, params.bottomMargin);
            padding = 0;
        } else {
            params.setMargins(0, params.topMargin, 0, params.bottomMargin);
        }
        setLayoutParams(params);

        // Navigation button should have more start padding in order to keep the navigation icon
        // and the list item icon aligned.
        int navigationButtonStartOffsetPx =
                mNavigationButton != NavigationButton.NONE ? mModernNavButtonStartOffsetPx : 0;

        int actionMenuBarEndOffsetPx =
                mIsSelectionEnabled
                        ? mModernToolbarActionMenuEndOffsetPx
                        : mModernToolbarSearchIconOffsetPx;

        this.setPaddingRelative(
                padding + paddingStartOffset + navigationButtonStartOffsetPx,
                this.getPaddingTop(),
                padding + actionMenuBarEndOffsetPx,
                this.getPaddingBottom());
    }

    /**
     * @return Whether search mode is currently active. Once a search is started, this method will
     *         return true until the search is ended regardless of whether the toolbar view changes
     *         dues to a selection.
     */
    public boolean isSearching() {
        assert mIsSearchingSupplier.get() != null : "Supplier is not correctly initialized.";
        return mIsSearchingSupplier.get();
    }

    public ObservableSupplier<Boolean> isSearchingSupplier() {
        return mIsSearchingSupplier;
    }

    SelectionDelegate<E> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    protected void showNormalView() {
        // hide overflow menu explicitly: crbug.com/999269
        hideOverflowMenu();

        mViewType = ViewType.NORMAL_VIEW;
        getMenu().setGroupVisible(mNormalGroupResId, true);
        getMenu().setGroupVisible(mSelectedGroupResId, false);
        if (mHasSearchView) {
            mSearchView.setVisibility(View.GONE);
            updateSearchMenuItem();
        }
        if (mShowBackInNormalView) {
            setNavigationButton(NavigationButton.NORMAL_VIEW_BACK);
        } else {
            setNavigationButton(NavigationButton.NONE);
        }
        setBackgroundColor(mNormalBackgroundColor);
        if (mTitleResId != 0) setTitle(mTitleResId);

        mNumberRollView.setVisibility(View.GONE);
        mNumberRollView.setNumber(0, false);

        updateDisplayStyleIfNecessary();
    }

    protected void showSelectionView(List<E> selectedItems, boolean wasSelectionEnabled) {
        mViewType = ViewType.SELECTION_VIEW;

        getMenu().setGroupVisible(mNormalGroupResId, false);
        getMenu().setGroupVisible(mSelectedGroupResId, true);
        getMenu().setGroupEnabled(mSelectedGroupResId, !selectedItems.isEmpty());
        if (mHasSearchView) mSearchView.setVisibility(View.GONE);

        setNavigationButton(NavigationButton.SELECTION_BACK);
        setBackgroundColor(mNormalBackgroundColor);

        switchToNumberRollView(selectedItems, wasSelectionEnabled);

        if (isSearching()) hideKeyboard();

        updateDisplayStyleIfNecessary();
    }

    private void showSearchViewInternal() {
        mViewType = ViewType.SEARCH_VIEW;

        getMenu().setGroupVisible(mNormalGroupResId, false);
        getMenu().setGroupVisible(mSelectedGroupResId, false);
        mNumberRollView.setVisibility(View.GONE);
        mSearchView.setVisibility(View.VISIBLE);

        setNavigationButton(NavigationButton.SEARCH_BACK);
        setBackgroundResource(R.drawable.search_toolbar_modern_bg);
        updateStatusBarColor(mSearchBackgroundColor);

        updateDisplayStyleIfNecessary();
    }

    private void updateSearchMenuItem() {
        if (!mHasSearchView) return;
        MenuItem searchMenuItem = getMenu().findItem(mSearchMenuItemId);
        if (searchMenuItem != null) {
            searchMenuItem.setVisible(mSearchEnabled && !mIsSelectionEnabled && !isSearching());
        }
    }

    protected void switchToNumberRollView(List<E> selectedItems, boolean wasSelectionEnabled) {
        setTitle(null);
        mNumberRollView.setVisibility(View.VISIBLE);
        if (!wasSelectionEnabled) mNumberRollView.setNumber(0, false);
        mNumberRollView.setNumber(selectedItems.size(), true);
    }

    private void updateDisplayStyleIfNecessary() {
        if (mUiConfig != null) onDisplayStyleChanged(mUiConfig.getCurrentDisplayStyle());
    }

    /**
     * Set info menu item used to toggle info header.
     *
     * @param infoMenuItemId The menu item to show or hide information.
     */
    public void setInfoMenuItem(@IdRes int infoMenuItemId) {
        mInfoMenuItemId = infoMenuItemId;
    }

    /**
     * Update icon, title, and visibility of info menu item.
     *
     * @param showItem Whether or not info menu item should show.
     * @param infoShowing Whether or not info header is currently showing.
     */
    public void updateInfoMenuItem(boolean showItem, boolean infoShowing) {
        MenuItem infoMenuItem = getMenu().findItem(mInfoMenuItemId);
        if (infoMenuItem != null) {
            if (mShowInfoIcon) {
                Drawable iconDrawable =
                        TintedDrawable.constructTintedDrawable(
                                getContext(),
                                R.drawable.btn_info,
                                infoShowing
                                        ? R.color.default_icon_color_accent1_tint_list
                                        : R.color.default_icon_color_secondary_tint_list);

                infoMenuItem.setIcon(iconDrawable);
            }

            infoMenuItem.setTitle(infoShowing ? mHideInfoStringId : mShowInfoStringId);
            infoMenuItem.setVisible(showItem);
        }
    }

    /** Hides the keyboard. */
    public void hideKeyboard() {
        KeyboardVisibilityDelegate.getInstance().hideKeyboard(mSearchEditText);
    }

    @Override
    public void setTitle(CharSequence title) {
        super.setTitle(title);

        // The super class adds an AppCompatTextView for the title which not focusable by default.
        // Set TextView children to focusable so the title can gain focus in accessibility mode.
        makeTextViewChildrenAccessible();
    }

    @Override
    public void setBackgroundColor(@ColorInt int color) {
        super.setBackgroundColor(color);

        updateStatusBarColor(color);
    }

    private void updateStatusBarColor(@ColorInt int color) {
        if (!mUpdateStatusBarColor) return;

        Context context = getContext();
        if (!(context instanceof Activity)) return;

        Window window = ((Activity) context).getWindow();
        UiUtils.setStatusBarColor(window, color);
        UiUtils.setStatusBarIconColor(
                window.getDecorView().getRootView(),
                !ColorUtils.shouldUseLightForegroundOnBackground(color));
    }

    public View getSearchViewForTests() {
        return mSearchView;
    }

    public @NavigationButton int getNavigationButtonForTests() {
        return mNavigationButton;
    }

    /** Ends any in-progress animations. */
    public void endAnimationsForTesting() {
        mNumberRollView.endAnimationsForTesting();
    }

    private void makeTextViewChildrenAccessible() {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (!(child instanceof TextView)) continue;
            child.setFocusable(true);

            // setFocusableInTouchMode is problematic for buttons, see
            // https://crbug.com/813422.
            if ((child instanceof Button)) continue;
            child.setFocusableInTouchMode(true);
        }
    }

    @VisibleForTesting
    public @ViewType int getCurrentViewType() {
        return mViewType;
    }
}
