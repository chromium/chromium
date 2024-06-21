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

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.MenuRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
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
    private TextView mEmptyView;
    private TextView mEmptyStateSubHeadingView;
    private View mEmptyViewWrapper;
    private ImageView mEmptyImageView;
    private LoadingView mLoadingView;
    private RecyclerView mRecyclerView;
    private ItemAnimator mItemAnimator;
    SelectableListToolbar<E> mToolbar;
    private FadingShadowView mToolbarShadow;

    private @StringRes int mEmptyStringResId;
    private CharSequence mEmptySubheadingString;

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
     * @param toolbarLayoutId The resource id of the toolbar layout. This will be inflated into a
     *     ViewStub.
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *     set a title when the selection is cleared.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *     established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *     established.
     * @param listener The OnMenuItemClickListener to set on the toolbar.
     * @param updateStatusBarColor Whether the status bar color should be updated to match the
     *     toolbar color. If true, the status bar will only be updated if the current device fully
     *     supports theming and is on Android M+.
     * @return The initialized SelectionToolbar.
     */
    public SelectableListToolbar<E> initializeToolbar(
            @LayoutRes int toolbarLayoutId,
            SelectionDelegate<E> delegate,
            @StringRes int titleResId,
            @IdRes int normalGroupResId,
            @IdRes int selectedGroupResId,
            @Nullable OnMenuItemClickListener listener,
            boolean updateStatusBarColor) {
        return initializeToolbar(
                toolbarLayoutId,
                delegate,
                titleResId,
                normalGroupResId,
                selectedGroupResId,
                listener,
                updateStatusBarColor,
                /* menuResId= */ 0,
                false);
    }

    /**
     * Initializes the SelectionToolbar with the option to show the back button in normal view.
     * #onNavigationBack must also be overridden in order to assign behavior to the button.
     *
     * @param toolbarLayoutId The resource id of the toolbar layout. This will be inflated into a
     *     ViewStub.
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *     set a title when the selection is cleared.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *     established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *     established.
     * @param listener The OnMenuItemClickListener to set on the toolbar.
     * @param updateStatusBarColor Whether the status bar color should be updated to match the
     *     toolbar color. If true, the status bar will only be updated if the current device fully
     *     supports theming and is on Android M+.
     * @param menuResId The resource id of the menu. {@code 0} if not required.
     * @param showBackInNormalView Whether the back arrow should appear on the normal view.
     * @return The initialized SelectionToolbar.
     */
    public SelectableListToolbar<E> initializeToolbar(
            @LayoutRes int toolbarLayoutId,
            SelectionDelegate<E> delegate,
            @StringRes int titleResId,
            @IdRes int normalGroupResId,
            @IdRes int selectedGroupResId,
            @Nullable OnMenuItemClickListener listener,
            boolean updateStatusBarColor,
            @MenuRes int menuResId,
            boolean showBackInNormalView) {
        mToolbarStub.setLayoutResource(toolbarLayoutId);
        @SuppressWarnings("unchecked")
        SelectableListToolbar<E> toolbar = (SelectableListToolbar<E>) mToolbarStub.inflate();
        mToolbar = toolbar;
        mToolbar.initialize(
                delegate,
                titleResId,
                normalGroupResId,
                selectedGroupResId,
                updateStatusBarColor,
                menuResId,
                showBackInNormalView);

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
    public TextView initializeEmptyView(@StringRes int emptyStringResId) {
        setEmptyViewText(emptyStringResId);

        // Empty listener to have the touch events dispatched to this view tree for navigation UI.
        mEmptyViewWrapper.setOnTouchListener((v, event) -> true);

        return mEmptyView;
    }

    /**
     * Initializes the empty state view with an image, heading, and subheading.
     *
     * @param imageResId Image view to show when the selectable list is empty.
     * @param emptyHeadingStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingString Subheading string to show when the selectable list is empty.
     * @return The {@link TextView} displayed when the list is empty.
     */
    // @TODO: (crbugs.com/1443648) Refactor return value for ForTesting method
    public TextView initializeEmptyStateView(
            @DrawableRes int imageResId,
            @StringRes int emptyHeadingStringResId,
            CharSequence emptySubheadingString) {
        // Initialize and inflate empty state view stub.
        ViewStub emptyViewStub = findViewById(R.id.empty_state_view_stub);
        View emptyStateView = emptyViewStub.inflate();

        // Initialize empty state resource.
        mEmptyView = emptyStateView.findViewById(R.id.empty_state_text_title);
        mEmptyStateSubHeadingView = emptyStateView.findViewById(R.id.empty_state_text_description);
        mEmptyImageView = emptyStateView.findViewById(R.id.empty_state_icon);
        mEmptyViewWrapper = emptyStateView.findViewById(R.id.empty_state_container);

        // Set empty state properties.
        setEmptyStateImageRes(imageResId);
        setEmptyStateViewText(emptyHeadingStringResId, emptySubheadingString);
        return mEmptyView;
    }

    /**
     * Sets the empty state view image when the selectable list is empty.
     *
     * @param imageResId The image view to show when the selectable list is empty.
     */
    public void setEmptyStateImageRes(@DrawableRes int imageResId) {
        mEmptyImageView.setImageResource(imageResId);
    }

    /**
     * Sets the view text when the selectable list is empty.
     *
     * @param emptyStringResId The string to show when the selectable list is empty.
     */
    public void setEmptyViewText(@StringRes int emptyStringResId) {
        mEmptyStringResId = emptyStringResId;

        mEmptyView.setText(mEmptyStringResId);
    }

    /**
     * Sets the view text when the selectable list is empty.
     *
     * @param emptyHeadingStringResId Heading string to show when the selectable list is empty.
     * @param emptySubheadingString Subheading string to show when the selectable list is empty.
     */
    public void setEmptyStateViewText(
            @StringRes int emptyHeadingStringResId, CharSequence emptySubheadingString) {
        mEmptyStringResId = emptyHeadingStringResId;
        mEmptySubheadingString = emptySubheadingString;

        mEmptyView.setText(mEmptyStringResId);
        mEmptyStateSubHeadingView.setText(mEmptySubheadingString);
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

    @Override
    public void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        int padding = getPaddingForDisplayStyle(newDisplayStyle, getResources());
        mRecyclerView.setPaddingRelative(
                padding, mRecyclerView.getPaddingTop(), padding, mRecyclerView.getPaddingBottom());
    }

    @Override
    public void onSelectionStateChange(List<E> selectedItems) {
        onBackPressStateChanged();
        setToolbarShadowVisibility();
    }

    /**
     * Called when a search is starting.
     *
     * @param searchEmptyString The string to show when the selectable list is empty during a
     *     search.
     * @param searchEmptySubheadingResId The resource ID of the string to show as the description.
     */
    public void onStartSearch(String searchEmptyString, @StringRes int searchEmptySubheadingResId) {
        mRecyclerView.setItemAnimator(null);
        mToolbarShadow.setVisibility(View.VISIBLE);
        mEmptyView.setText(searchEmptyString);
        if (searchEmptySubheadingResId != Resources.ID_NULL) {
            mEmptyStateSubHeadingView.setText(searchEmptySubheadingResId);
        }
        onBackPressStateChanged();
    }

    /** Called when a search has ended. */
    public void onEndSearch() {
        mRecyclerView.setItemAnimator(mItemAnimator);
        setToolbarShadowVisibility();
        mEmptyView.setText(mEmptyStringResId);
        mEmptyStateSubHeadingView.setText(mEmptySubheadingString);

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
        mEmptyView.setVisibility(visible);
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
