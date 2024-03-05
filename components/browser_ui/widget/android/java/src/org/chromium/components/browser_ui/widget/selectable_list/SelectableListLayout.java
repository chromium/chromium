// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.AdapterDataObserver;
import androidx.recyclerview.widget.RecyclerView.ItemAnimator;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.ui.widget.LoadingView;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains UI elements common to selectable list views: a loading view, empty view, selection
 * toolbar, shadow, and RecyclerView.
 *
 * <p>After the SelectableListLayout is inflated, it should be initialized through calls to
 * #initializeRecyclerView(), #initializeToolbar(), and #initializeEmptyStateView().
 * #initializeEmptyStateView() must be initialized after #initializeRecyclerView() to accommodate
 * items in recycler view that do not determine whether the empty view should be displayed, e.g.
 * bookmark toolbar. Currently the height of these views are calculated manually and set it as the
 * top margin for the empty view to prevent any overlap.
 *
 * <p>Must call #onDestroyed() to destroy SelectableListLayout properly, otherwise this would cause
 * memory leak consistently.
 *
 * @param <E> The type of the selectable items this layout holds.
 */
public class SelectableListLayout<E> extends FrameLayout
        implements DisplayStyleObserver, SelectionObserver<E>, BackPressHandler {
    private static final int WIDE_DISPLAY_MIN_PADDING_DP = 16;
    private RecyclerView.Adapter mAdapter;
    private ViewStub mToolbarStub;
    private TextView mEmptyHeadingTextView;
    private TextView mEmptySubHeadingTextView;
    private View mEmptyViewWrapper;
    private ImageView mEmptyImageView;
    private LoadingView mLoadingView;
    private RecyclerView mRecyclerView;
    private FrameLayout mContentView;
    private ItemAnimator mItemAnimator;
    SelectableListToolbar<E> mToolbar;
    private FadingShadowView mToolbarShadow;

    private int mEmptyStringResId;
    private UiConfig mUiConfig;

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final Set<Integer> mIgnoredTypesForEmptyState = new HashSet<>();

    private final AdapterDataObserver mAdapterObserver =
            new AdapterDataObserver() {
                @Override
                public void onChanged() {
                    super.onChanged();
                    updateLayout();
                    // At inflation, the RecyclerView is set to gone, and the loading view is
                    // visible. As long as the adapter data changes, we show the recycler view,
                    // and hide loading view.
                    mLoadingView.hideLoadingUI();
                }

                @Override
                public void onItemRangeInserted(int positionStart, int itemCount) {
                    super.onItemRangeInserted(positionStart, itemCount);
                    updateLayout();
                    // At inflation, the RecyclerView is set to gone, and the loading view is
                    // visible. As long as the adapter data changes, we show the recycler view,
                    // and hide loading view.
                    mLoadingView.hideLoadingUI();
                }

                @Override
                public void onItemRangeRemoved(int positionStart, int itemCount) {
                    super.onItemRangeRemoved(positionStart, itemCount);
                    updateLayout();
                }
            };

    public SelectableListLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        onBackPressStateChanged(); // Initialize back press state.
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LayoutInflater.from(getContext()).inflate(R.layout.selectable_list_layout, this);

        mEmptyHeadingTextView = findViewById(R.id.empty_view);
        mEmptyViewWrapper = findViewById(R.id.empty_view_wrapper);
        mLoadingView = findViewById(R.id.loading_view);
        mLoadingView.showLoadingUI();

        mToolbarStub = findViewById(R.id.action_bar_stub);

        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mUiConfig != null) mUiConfig.updateDisplayStyle();
    }

    /**
     * Creates a RecyclerView for the given adapter.
     *
     * @param adapter The adapter that provides a binding from an app-specific data set to views
     *                that are displayed within the RecyclerView.
     * @return The RecyclerView itself.
     */
    public RecyclerView initializeRecyclerView(RecyclerView.Adapter adapter) {
        return initializeRecyclerView(adapter, null);
    }

    /**
     * Initializes the layout with the given recycler view and adapter.
     *
     * @param adapter The adapter that provides a binding from an app-specific data set to views
     *                that are displayed within the RecyclerView.
     * @param recyclerView The recycler view to be shown.
     * @return The RecyclerView itself.
     */
    public RecyclerView initializeRecyclerView(
            RecyclerView.Adapter adapter, @Nullable RecyclerView recyclerView) {
        mAdapter = adapter;

        if (recyclerView == null) {
            mRecyclerView = findViewById(R.id.selectable_list_recycler_view);
            mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        } else {
            mRecyclerView = recyclerView;

            // Replace the inflated recycler view with the one supplied to this method.
            mContentView = findViewById(R.id.list_content);
            RecyclerView existingView =
                    mContentView.findViewById(R.id.selectable_list_recycler_view);
            mContentView.removeView(existingView);
            mContentView.addView(mRecyclerView, 0);
        }

        mRecyclerView.setAdapter(mAdapter);
        initializeRecyclerViewProperties();
        return mRecyclerView;
    }

    private void initializeRecyclerViewProperties() {
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.addOnScrollListener(
                new OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        setToolbarShadowVisibility();
                    }
                });
        mRecyclerView.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> {
                    setToolbarShadowVisibility();
                });

        mItemAnimator = mRecyclerView.getItemAnimator();
    }

    /**
     * Initializes the SelectionToolbar.
     *
     * @param toolbarLayoutId The resource id of the toolbar layout. This will be inflated into
     *                        a ViewStub.
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *                   set a title when the selection is cleared.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *                         established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *                           established.
     * @param listener The OnMenuItemClickListener to set on the toolbar.
     * @param updateStatusBarColor Whether the status bar color should be updated to match the
     *                             toolbar color. If true, the status bar will only be updated if
     *                             the current device fully supports theming and is on Android M+.
     * @return The initialized SelectionToolbar.
     */
    public SelectableListToolbar<E> initializeToolbar(
            int toolbarLayoutId,
            SelectionDelegate<E> delegate,
            int titleResId,
            int normalGroupResId,
            int selectedGroupResId,
            @Nullable OnMenuItemClickListener listener,
            boolean updateStatusBarColor) {
        mToolbarStub.setLayoutResource(toolbarLayoutId);
        @SuppressWarnings("unchecked")
        SelectableListToolbar<E> toolbar = (SelectableListToolbar<E>) mToolbarStub.inflate();
        mToolbar = toolbar;
        mToolbar.initialize(
                delegate, titleResId, normalGroupResId, selectedGroupResId, updateStatusBarColor);

        if (listener != null) {
            mToolbar.setOnMenuItemClickListener(listener);
        }

        mToolbarShadow = findViewById(R.id.shadow);
        mToolbarShadow.init(
                getContext().getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);

        delegate.addObserver(this);
        setToolbarShadowVisibility();

        return mToolbar;
    }

    /**
     * Initializes the view shown when the selectable list is empty.
     *
     * @param emptyStringResId The string to show when the selectable list is empty.
     * @return The {@link TextView} displayed when the list is empty.
     */
    public TextView initializeEmptyView(int emptyStringResId) {
        setEmptyViewText(emptyStringResId);

        // Empty listener to have the touch events dispatched to this view tree for navigation UI.
        mEmptyViewWrapper.setOnTouchListener((v, event) -> true);

        return mEmptyHeadingTextView;
    }

    /**
     * Initializes the empty state view with an image, heading, and subheading.
     *
     * @param emptyImageResId Image view to show when the selectable list is empty.
     * @param emptyHeadingStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingStringResId SubString to show when the selectable list is empty.
     * @return The empty view displayed when the list is empty.
     */
    public View initializeEmptyStateView(
            int emptyImageResId, int emptyHeadingStringResId, int emptySubheadingStringResId) {
        // Initialize and inflate empty state view stub.
        ViewStub emptyViewStub = findViewById(R.id.empty_state_view_stub);
        View emptyStateView = emptyViewStub.inflate();

        // Initialize empty state resource.
        mEmptyHeadingTextView = emptyStateView.findViewById(R.id.empty_state_text_title);
        mEmptySubHeadingTextView = emptyStateView.findViewById(R.id.empty_state_text_description);
        mEmptyImageView = emptyStateView.findViewById(R.id.empty_state_icon);
        mEmptyViewWrapper = emptyStateView.findViewById(R.id.empty_state_container);

        // Adjust empty view margin and position based on available space.
        addMarginOnSizeChanged();

        // Set empty state properties.
        setEmptyStateImageRes(emptyImageResId);
        setEmptyStateViewText(emptyHeadingStringResId, emptySubheadingStringResId);
        return mEmptyViewWrapper;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        addMarginOnSizeChanged();
    }

    // TODO:(crbug.com/328128383) Try use RelativeLayout for list_content, so there is no need for
    // this manual calculation.
    private void addMarginOnSizeChanged() {
        if (mAdapter == null || mEmptyViewWrapper == null || !isListEffectivelyEmpty()) {
            return;
        }

        ViewGroup emptyScrollView = mEmptyViewWrapper.findViewById(R.id.empty_state_container);
        if (emptyScrollView == null) {
            return;
        }

        mContentView = findViewById(R.id.list_content);
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mEmptyViewWrapper.getLayoutParams();

        int mListContentHeight = mContentView.getHeight();
        int topMargin = 0;

        // Loop to identify views above the empty view in mContentView and accumulate them as a top
        // margin to prevent overlap.
        for (int i = 0; i < mContentView.getChildCount(); i++) {
            ViewGroup childView = (ViewGroup) mContentView.getChildAt(i);
            if (childView.equals(mEmptyViewWrapper)) {
                break;
            }
            for (int j = 0; j < childView.getChildCount(); j++) {
                ViewGroup.MarginLayoutParams marginLayoutParams =
                        (ViewGroup.MarginLayoutParams) childView.getChildAt(j).getLayoutParams();
                topMargin += childView.getChildAt(j).getHeight() + marginLayoutParams.topMargin;

                // Do not accumulate the bottom margin when it is the last view above an empty view
                // to avoid additional gaps.
                if (j < childView.getChildCount() - 1
                        || (i < mContentView.getChildCount() - 1)
                                && !mContentView.getChildAt(i + 1).equals(mEmptyViewWrapper)) {
                    topMargin += marginLayoutParams.bottomMargin;
                }
            }
        }

        int bottomMargin = topMargin;

        // Empty view LinearLayout full height with max margins.
        int maxEmptyHeight = emptyScrollView.getChildAt(0).getHeight() + bottomMargin + topMargin;

        // Decide gravity and margin based on available space.
        if (mListContentHeight < maxEmptyHeight) {
            layoutParams.setMargins(0, topMargin, 0, 0);
            layoutParams.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        } else {
            // Adding a top margin requires a bottom margin to center the empty view.
            layoutParams.setMargins(0, topMargin, 0, bottomMargin);
            layoutParams.gravity = Gravity.CENTER;
        }
        mEmptyViewWrapper.setLayoutParams(layoutParams);
    }

    /**
     * Sets the empty state view image when the selectable list is empty.
     * @param imageResId The image view to show when the selectable list is empty.
     */
    public void setEmptyStateImageRes(int imageResId) {
        mEmptyImageView.setImageResource(imageResId);
    }

    /**
     * Sets the view text when the selectable list is empty.
     * @param emptyStringResId The string to show when the selectable list is empty.
     */
    public void setEmptyViewText(int emptyStringResId) {
        mEmptyStringResId = emptyStringResId;

        mEmptyHeadingTextView.setText(mEmptyStringResId);
    }

    /**
     * Sets the view text when the selectable list is empty.
     * @param emptyStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingStringResId SubString to show when the selectable list is empty.
     */
    public void setEmptyStateViewText(int emptyHeadingStringResId, int emptySubheadingStringResId) {
        mEmptyStringResId = emptyHeadingStringResId;

        mEmptyHeadingTextView.setText(mEmptyStringResId);
        mEmptySubHeadingTextView.setText(emptySubheadingStringResId);
    }

    /**
     * Adds the given type to the set of ignored item types. Items of this type in the adapter won't
     * be counted when deciding to show the empty state view.
     */
    public void ignoreItemTypeForEmptyState(int type) {
        mIgnoredTypesForEmptyState.add(type);
    }

    /** Called when the view that owns the SelectableListLayout is destroyed. */
    public void onDestroyed() {
        mAdapter.unregisterAdapterDataObserver(mAdapterObserver);
        mToolbar.getSelectionDelegate().removeObserver(this);
        mToolbar.destroy();
        mLoadingView.destroy();
        mRecyclerView.setAdapter(null);
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the SelectableListLayout will be visually centered
     * by adding padding to both sides.
     *
     * This method should be called after the toolbar and RecyclerView are initialized.
     */
    public void configureWideDisplayStyle() {
        mUiConfig = new UiConfig(this);
        mToolbar.configureWideDisplayStyle(mUiConfig);
        mUiConfig.addObserver(this);
    }

    /** @return The {@link UiConfig} associated with this View if one has been created, or null. */
    @Nullable
    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    @Override
    public void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        int padding = getPaddingForDisplayStyle(newDisplayStyle, getResources());

        ViewCompat.setPaddingRelative(
                mRecyclerView,
                padding,
                mRecyclerView.getPaddingTop(),
                padding,
                mRecyclerView.getPaddingBottom());
    }

    @Override
    public void onSelectionStateChange(List<E> selectedItems) {
        onBackPressStateChanged();
        setToolbarShadowVisibility();
    }

    /**
     * Called when a search is starting.
     * @param searchEmptyStringResId The string to show when the selectable list is empty during a
     *         search.
     */
    public void onStartSearch(@StringRes int searchEmptyStringResId) {
        onStartSearch(getContext().getString(searchEmptyStringResId));
    }

    /**
     * Called when a search is starting.
     * @param searchEmptyString The string to show when the selectable list is empty during a
     *         search.
     */
    public void onStartSearch(String searchEmptyString) {
        mRecyclerView.setItemAnimator(null);
        mToolbarShadow.setVisibility(View.VISIBLE);
        mEmptyHeadingTextView.setText(searchEmptyString);
        onBackPressStateChanged();
    }

    /** Called when a search has ended. */
    public void onEndSearch() {
        mRecyclerView.setItemAnimator(mItemAnimator);
        setToolbarShadowVisibility();
        mEmptyHeadingTextView.setText(mEmptyStringResId);
        onBackPressStateChanged();
    }

    /**
     * @param displayStyle The current display style..
     * @param resources The {@link Resources} used to retrieve configuration and display metrics.
     * @return The lateral padding to use for the current display style.
     */
    public static int getPaddingForDisplayStyle(DisplayStyle displayStyle, Resources resources) {
        int padding = 0;
        if (displayStyle.horizontal == HorizontalDisplayStyle.WIDE) {
            int screenWidthDp = resources.getConfiguration().screenWidthDp;
            float dpToPx = resources.getDisplayMetrics().density;
            padding =
                    (int)
                            (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
                                    * dpToPx);
            padding = (int) Math.max(WIDE_DISPLAY_MIN_PADDING_DP * dpToPx, padding);
        }
        return padding;
    }

    private void setToolbarShadowVisibility() {
        if (mToolbar == null || mRecyclerView == null) return;

        boolean showShadow = mRecyclerView.canScrollVertically(-1);
        mToolbarShadow.setVisibility(showShadow ? View.VISIBLE : View.GONE);
    }

    /**
     * Unlike ListView or GridView, RecyclerView does not provide default empty
     * view implementation. We need to check it ourselves.
     */
    private void updateEmptyViewVisibility() {
        int visible = isListEffectivelyEmpty() ? View.VISIBLE : View.GONE;
        mEmptyHeadingTextView.setVisibility(visible);
        mEmptyViewWrapper.setVisibility(visible);
    }

    /**
     * For efficiency, only loop over the items if there are ignored types present in the set and
     * bail on the loop as soon as one is detected.
     */
    private boolean isListEffectivelyEmpty() {
        if (mIgnoredTypesForEmptyState.isEmpty()) {
            return mAdapter.getItemCount() == 0;
        }

        for (int i = 0; i < mAdapter.getItemCount(); i++) {
            if (!mIgnoredTypesForEmptyState.contains(mAdapter.getItemViewType(i))) {
                return false;
            }
        }

        return true;
    }

    private void updateLayout() {
        updateEmptyViewVisibility();
        if (mAdapter.getItemCount() == 0) {
            mRecyclerView.setVisibility(View.GONE);
        } else {
            mRecyclerView.setVisibility(View.VISIBLE);
        }

        mToolbar.setSearchEnabled(mAdapter.getItemCount() != 0);
    }

    public View getToolbarShadowForTests() {
        return mToolbarShadow;
    }

    /**
     * Called when the user presses the back key. Note that this method is not called automatically.
     * The embedding UI must call this method
     * when a backpress is detected for the event to be handled.
     * @return Whether this event is handled.
     */
    public boolean onBackPressed() {
        SelectionDelegate selectionDelegate = mToolbar.getSelectionDelegate();
        if (selectionDelegate.isSelectionEnabled()) {
            selectionDelegate.clearSelection();
            return true;
        }

        if (mToolbar.isSearching()) {
            mToolbar.hideSearchView();
            return true;
        }

        return false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        var ret = onBackPressed();
        assert ret;
        return ret ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    private void onBackPressStateChanged() {
        if (mToolbar == null) {
            mBackPressStateSupplier.set(false);
            return;
        }
        mBackPressStateSupplier.set(
                mToolbar.getSelectionDelegate().isSelectionEnabled() || mToolbar.isSearching());
    }
}
