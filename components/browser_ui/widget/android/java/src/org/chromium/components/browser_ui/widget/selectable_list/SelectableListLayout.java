// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
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

import java.util.List;

/**
 * Contains UI elements common to selectable list views: a loading view, empty view, selection
 * toolbar, shadow, and RecyclerView.
 *
 * After the SelectableListLayout is inflated, it should be initialized through calls to
 * #initializeRecyclerView(), #initializeToolbar(), and #initializeEmptyView().
 *
 * Must call #onDestroyed() to destroy SelectableListLayout properly, otherwise this would cause
 * memory leak consistently.
 *
 * @param <E> The type of the selectable items this layout holds.
 */
public class SelectableListLayout<E> extends FrameLayout
        implements DisplayStyleObserver, SelectionObserver<E>, BackPressHandler {
    private static final int WIDE_DISPLAY_MIN_PADDING_DP = 16;
    private RecyclerView.Adapter mAdapter;
    private ViewStub mToolbarStub;
    private TextView mEmptyView;
    private TextView mEmptyStateSubHeadingView;
    private View mEmptyViewWrapper;
    private ImageView mEmptyImageView;
    private LoadingView mLoadingView;
    private RecyclerView mRecyclerView;
    private ItemAnimator mItemAnimator;
    SelectableListToolbar<E> mToolbar;
    private FadingShadowView mToolbarShadow;

    private int mEmptyStringResId;

    private UiConfig mUiConfig;

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private final AdapterDataObserver mAdapterObserver = new AdapterDataObserver() {
        @Override
        public void onChanged() {
            super.onChanged();
            updateLayout();
            // At inflation, the RecyclerView is set to gone, and the loading view is visible. As
            // long as the adapter data changes, we show the recycler view, and hide loading view.
            mLoadingView.hideLoadingUI();
        }

        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            super.onItemRangeInserted(positionStart, itemCount);
            updateLayout();
            // At inflation, the RecyclerView is set to gone, and the loading view is visible. As
            // long as the adapter data changes, we show the recycler view, and hide loading view.
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

        mEmptyView = findViewById(R.id.empty_view);
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
            FrameLayout contentView = findViewById(R.id.list_content);
            RecyclerView existingView =
                    contentView.findViewById(R.id.selectable_list_recycler_view);
            contentView.removeView(existingView);
            contentView.addView(mRecyclerView, 0);
        }

        mRecyclerView.setAdapter(mAdapter);
        initializeRecyclerViewProperties();
        return mRecyclerView;
    }

    private void initializeRecyclerViewProperties() {
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                setToolbarShadowVisibility();
            }
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
    public SelectableListToolbar<E> initializeToolbar(int toolbarLayoutId,
            SelectionDelegate<E> delegate, int titleResId, int normalGroupResId,
            int selectedGroupResId, @Nullable OnMenuItemClickListener listener,
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

        return mEmptyView;
    }

    /**
     * Initializes the empty state view with an image, heading, and subheading.
     * @param imageResId Image view to show when the selectable list is empty.
     * @param emptyHeadingStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingStringResId SubString to show when the selectable list is empty.
     * @return The {@link TextView} displayed when the list is empty.
     */
    // @TODO: (crbugs.com/1443648) Refactor return value for ForTesting method
    public TextView initializeEmptyStateView(
            int imageResId, int emptyHeadingStringResId, int emptySubheadingStringResId) {
        // Initialize and inflate empty state view stub.
        ViewStub emptyViewStub = findViewById(R.id.empty_state_view_stub);
        View emptyStateView = emptyViewStub.inflate();
        int bottomMargin = getContext().getResources().getDimensionPixelSize(
                                   R.dimen.selectable_list_toolbar_height)
                / 2;
        FrameLayout.LayoutParams emptyViewParams =
                (FrameLayout.LayoutParams) emptyStateView.getLayoutParams();
        emptyViewParams.bottomMargin = bottomMargin;
        emptyStateView.setLayoutParams(emptyViewParams);

        // Initialize empty state resource.
        mEmptyView = emptyStateView.findViewById(R.id.empty_state_text_title);
        mEmptyStateSubHeadingView = emptyStateView.findViewById(R.id.empty_state_text_description);
        mEmptyImageView = emptyStateView.findViewById(R.id.empty_state_icon);
        mEmptyViewWrapper = emptyStateView.findViewById(R.id.empty_state_container);

        // Set empty state properties.
        setEmptyStateImageRes(imageResId);
        setEmptyStateViewText(emptyHeadingStringResId, emptySubheadingStringResId);
        return mEmptyView;
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

        mEmptyView.setText(mEmptyStringResId);
    }

    /**
     * Sets the view text when the selectable list is empty.
     * @param emptyStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingStringResId SubString to show when the selectable list is empty.
     */
    public void setEmptyStateViewText(int emptyHeadingStringResId, int emptySubheadingStringResId) {
        mEmptyStringResId = emptyHeadingStringResId;

        mEmptyView.setText(mEmptyStringResId);
        mEmptyStateSubHeadingView.setText(emptySubheadingStringResId);
    }

    /**
     * Called when the view that owns the SelectableListLayout is destroyed.
     */
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

    /**
     * @return The {@link UiConfig} associated with this View if one has been created, or null.
     */
    @Nullable
    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    @Override
    public void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        int padding = getPaddingForDisplayStyle(newDisplayStyle, getResources());

        ViewCompat.setPaddingRelative(mRecyclerView, padding, mRecyclerView.getPaddingTop(),
                padding, mRecyclerView.getPaddingBottom());
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
        mEmptyView.setText(searchEmptyString);
        onBackPressStateChanged();
    }

    /**
     * Called when a search has ended.
     */
    public void onEndSearch() {
        mRecyclerView.setItemAnimator(mItemAnimator);
        setToolbarShadowVisibility();
        mEmptyView.setText(mEmptyStringResId);
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
            padding = (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
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
        int visible = mAdapter.getItemCount() == 0 ? View.VISIBLE : View.GONE;
        mEmptyView.setVisibility(visible);
        mEmptyViewWrapper.setVisibility(visible);
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
