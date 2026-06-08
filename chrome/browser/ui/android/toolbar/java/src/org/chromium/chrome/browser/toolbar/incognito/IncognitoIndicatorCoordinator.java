// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.incognito;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.animation.Animator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarChild;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Collection;
import java.util.function.Supplier;

@NullMarked
public class IncognitoIndicatorCoordinator extends ToolbarChild
        implements View.OnClickListener, View.OnLongClickListener, View.OnContextClickListener {
    private final ToolbarLayout mParentToolbar;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private @Nullable Boolean mIsIncognitoBranded;
    private boolean mVisible;
    private @Nullable View mIncognitoIndicator;
    private final int mDefaultFallbackWidth;
    private int mCachedWidth;
    private @Nullable AnchoredPopupWindow mMenuWindow;
    private final Supplier<Integer> mIncognitoWindowCountSupplier;

    /**
     * Creates an IncognitoIndicatorCoordinator for managing the incognito indicator on the top
     * toolbar.
     *
     * @param parentToolbar The parent view that contains the incognito indicator.
     * @param userEducationHelper Helper for triggering in-product help (IPH).
     * @param trackerSupplier A supplier for the tracker to record feature usage event.
     * @param topUiThemeColorProvider Provides theme and tint color that should be applied to the
     *     view.
     * @param incognitoStateProvider Provides incognito state to update view.
     * @param incognitoWindowCountSupplier A supplier for the number of incognito windows.
     * @param visible Whether the toolbar buttons should start out being visible.
     */
    public IncognitoIndicatorCoordinator(
            ToolbarLayout parentToolbar,
            UserEducationHelper userEducationHelper,
            Supplier<@Nullable Tracker> trackerSupplier,
            ThemeColorProvider topUiThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            Supplier<Integer> incognitoWindowCountSupplier,
            boolean visible) {
        super(topUiThemeColorProvider, incognitoStateProvider);
        mParentToolbar = parentToolbar;
        mUserEducationHelper = userEducationHelper;
        mTrackerSupplier = trackerSupplier;
        mVisible = visible;
        mIncognitoWindowCountSupplier = incognitoWindowCountSupplier;
        setVisibility(mVisible);

        // Use a width of three toolbar buttons as a fallback for displaying the incognito
        // indicator.
        int buttonWidth =
                mParentToolbar
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        mDefaultFallbackWidth = 3 * buttonWidth;
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        super.onIncognitoStateChanged(isIncognito);
        if (mIsIncognitoBranded != null && mIsIncognitoBranded == isIncognito) return;

        mIsIncognitoBranded = isIncognito;
        setVisibility(mVisible);
    }

    @Override
    public boolean isVisible() {
        return mIncognitoIndicator != null && mIncognitoIndicator.getVisibility() == View.VISIBLE;
    }

    /**
     * Updates the visibility of the incognito indicator.
     *
     * @param visible Whether the toolbar buttons are visible.
     */
    public void setVisibility(boolean visible) {
        mVisible = visible;
        boolean wasVisible = isVisible();

        // If the parent toolbar is null, this was called before initialization was completed. Skip
        // for now and wait until a subsequent call after initialization has finished.
        if (mParentToolbar == null) return;
        if (mIsIncognitoBranded == null) return;
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) return;

        if (mIncognitoIndicator == null && mIsIncognitoBranded) {
            ViewStub stub = mParentToolbar.findViewById(R.id.incognito_indicator_stub);
            mIncognitoIndicator = stub.inflate();
            mIncognitoIndicator.setOnClickListener(this);
            mIncognitoIndicator.setOnLongClickListener(this);
            mIncognitoIndicator.setOnContextClickListener(this);
        }

        if (mIncognitoIndicator != null) {
            mIncognitoIndicator.setVisibility(mIsIncognitoBranded && visible ? VISIBLE : GONE);
            if (isVisible() && !wasVisible) {
                triggerIPH();
            }
        }
    }

    @Override
    public void draw(View root, Canvas canvas) {
        throw new UnsupportedOperationException("This method call is not yet supported.");
    }

    @Override
    public int updateVisibility(int availableWidth) {
        return updateVisibility(
                availableWidth,
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
    }

    @Override
    public int updateVisibility(int availableWidth, int widthMeasureSpec, int heightMeasureSpec) {
        assert ToolbarUtils.isToolbarTabletResizeRefactorEnabled();
        // Hide and consume no width if the desktop-like incognito window feature is not enabled,
        // or if the device is not in incognito mode. Do not cache the width of the indicator.
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()
                || Boolean.FALSE.equals(mIsIncognitoBranded)) {
            setVisibility(false);
            return 0;
        }

        // If the incognito indicator has been displayed, cache its measured width.
        if (mIncognitoIndicator != null) {
            // Actively measure the view content to get the "Stable Width" upfront.
            // This mimics the "Fixed Width" behavior of other buttons by determining the
            // intrinsic size immediately.
            // Use the parent's measure specs to mimic the actual measurement that will happen
            // in ToolbarTablet.onMeasure.
            ViewGroup.LayoutParams lp = mIncognitoIndicator.getLayoutParams();
            int childWidthSpec =
                    ViewGroup.getChildMeasureSpec(
                            widthMeasureSpec,
                            mParentToolbar.getPaddingLeft() + mParentToolbar.getPaddingRight(),
                            lp.width);
            int childHeightSpec =
                    ViewGroup.getChildMeasureSpec(
                            heightMeasureSpec,
                            mParentToolbar.getPaddingTop() + mParentToolbar.getPaddingBottom(),
                            lp.height);

            mIncognitoIndicator.measure(childWidthSpec, childHeightSpec);
            int measuredWidth = mIncognitoIndicator.getMeasuredWidth();
            mCachedWidth = measuredWidth > 0 ? measuredWidth : mDefaultFallbackWidth;
        } else {
            mCachedWidth = mDefaultFallbackWidth;
        }

        // Only display the indicator if there is enough available width. If the available width
        // is less than necessary, though, that extra width should still be consumed to avoid
        // showing any more buttons, as it might be confusing to users. This extra width will end up
        // absorbed into the location bar.
        setVisibility(availableWidth >= mCachedWidth);
        return Math.min(availableWidth, mCachedWidth);
    }

    @Override
    public int updateVisibilityWithAnimation(int availableWidth, Collection<Animator> animators) {
        return updateVisibility(availableWidth);
    }

    public @Nullable View getIncognitoIndicatorView() {
        return mIncognitoIndicator;
    }

    public @Nullable AnchoredPopupWindow getMenuWindowForTesting() {
        return mMenuWindow;
    }

    /**
     * Create and display the close Incognito windows menu anchored to the Incognito indicator.
     *
     * @param context The context of the Close Incognito windows menu.
     * @param listItems The menu item models.
     */
    protected void createAndShowMenu(Context context, ModelList listItems) {
        if (mIncognitoIndicator == null) return;

        Delegate delegate =
                (model, view) -> {
                    int menuId = model.get(ListMenuItemProperties.MENU_ITEM_ID);
                    if (menuId == R.id.close_all_incognito_windows_menu_id) {
                        RecordUserAction.record("MobileIncognitoIndicatorCloseAllWindows");
                        IncognitoTabHostUtils.closeAllIncognitoTabs();
                    }
                    if (mMenuWindow != null) {
                        mMenuWindow.dismiss();
                    }
                };

        BasicListMenu menu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context,
                        listItems,
                        delegate,
                        R.drawable.popup_menu_bg_no_horizontal_padding,
                        R.color.toolbar_text_box_background_incognito,
                        null);

        int measuredWidth = menu.getMenuDimensions()[0];
        mMenuWindow =
                new AnchoredPopupWindow.Builder(
                                context,
                                mIncognitoIndicator,
                                new ColorDrawable(Color.TRANSPARENT),
                                menu::getContentView,
                                new ViewRectProvider(mIncognitoIndicator))
                        .addOnDismissListener(
                                () -> {
                                    if (mIncognitoIndicator != null) {
                                        mIncognitoIndicator.setSelected(false);
                                    }
                                })
                        .setAnimateFromAnchor(true)
                        .setDismissOnScreenSizeChange(true)
                        .setDismissOnTouchInteraction(true)
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setDesiredContentWidth(measuredWidth)
                        .setHorizontalOverlapAnchor(true)
                        .setPreferredHorizontalOrientation(
                                AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE)
                        .setVerticalOverlapAnchor(false)
                        .build();

        mMenuWindow.show();
    }

    @VisibleForTesting
    ModelList buildMenuItems(Context context) {
        int incognitoWindowCount = mIncognitoWindowCountSupplier.get();
        String title =
                context.getResources()
                        .getQuantityString(
                                R.plurals.menu_close_all_incognito_windows,
                                incognitoWindowCount,
                                incognitoWindowCount);
        ModelList itemList = new ModelList();
        itemList.add(
                new ListItemBuilder()
                        .withTitle(title)
                        .withMenuId(R.id.close_all_incognito_windows_menu_id)
                        .withIsIncognito(true)
                        .build());
        return itemList;
    }

    @Override
    public void onClick(View view) {
        if (mIncognitoIndicator == null || mIncognitoIndicator.getVisibility() != View.VISIBLE) {
            return;
        }

        if (mTrackerSupplier.get() != null) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.INCOGNITO_INDICATOR_CLOSE_ALL_WINDOWS_USED);
        }

        if (mMenuWindow != null && mMenuWindow.isShowing()) {
            mMenuWindow.dismiss();
            return;
        }
        RecordUserAction.record("MobileIncognitoIndicatorClicked");
        Context context = mIncognitoIndicator.getContext();
        mIncognitoIndicator.setSelected(true);
        createAndShowMenu(context, buildMenuItems(context));
    }

    @Override
    public boolean onLongClick(View view) {
        onClick(view);
        return true;
    }

    @Override
    public boolean onContextClick(View view) {
        onClick(view);
        return true;
    }

    private void triggerIPH() {
        if (mIncognitoIndicator == null) return;
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mIncognitoIndicator.getResources(),
                                FeatureConstants.IPH_INCOGNITO_INDICATOR_CLOSE_ALL_WINDOWS,
                                R.string.iph_incognito_indicator_close_all_windows_text,
                                R.string
                                        .iph_incognito_indicator_close_all_windows_accessibility_text)
                        .setAnchorView(mIncognitoIndicator)
                        .build());
    }
}
