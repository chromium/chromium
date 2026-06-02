// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.graphics.drawable.ColorDrawable;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewStub;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/** The top-level component responsible for the setup and lifecycle of the PDF Toolbar MVC stack. */
@NullMarked
public class PdfToolbarCoordinator implements View.OnClickListener, View.OnKeyListener {
    private static final float THRESHOLD_DOWNLOAD_DP = 800f;
    private static final float THRESHOLD_ROTATE_DP = 750f;
    private static final float THRESHOLD_FIT_DP = 700f;
    private static final float THRESHOLD_ZOOM_DP = 650f;
    private static final float THRESHOLD_NAV_EDIT_DP = 600f;

    private final PropertyModel mModel;
    private final PdfToolbarActionsDelegate mDelegate;
    private final PropertyModelChangeProcessor<PropertyModel, PdfToolbar, PropertyKey>
            mPropertyModelChangeProcessor;
    private final List<Float> mZoomLevels =
            List.of(
                    0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f,
                    2.0f, 2.5f, 3.0f, 4.0f, 5.0f);
    private @Nullable AnchoredPopupWindow mMenuWindow;
    private @Nullable PdfToolbar mToolbar;

    public PdfToolbarCoordinator(View parentView, PdfToolbarActionsDelegate delegate) {
        mDelegate = delegate;
        PdfToolbar toolbar = parentView.findViewById(R.id.pdf_toolbar);
        if (toolbar == null) {
            ViewStub stub = parentView.findViewById(R.id.pdf_toolbar_stub);
            assert stub != null;
            toolbar = (PdfToolbar) stub.inflate();
        }
        toolbar.setVisibility(View.VISIBLE);
        mToolbar = toolbar;

        // TODO(crbug.com/507061296): Remove hardcoded values after the PDF is loaded.
        mModel =
                new PropertyModel.Builder(PdfToolbarProperties.ALL_KEYS)
                        .with(PdfToolbarProperties.ON_CLICK_LISTENER, this)
                        .with(
                                PdfToolbarProperties.PAGE_NUMBER_EDIT_LISTENER,
                                this::onPageNumberSubmitted)
                        .with(PdfToolbarProperties.ZOOM_LEVEL, 1.0f)
                        .with(PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON, true)
                        .with(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE, false)
                        .with(PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE, true)
                        .with(PdfToolbarProperties.ROTATE_BUTTON_VISIBLE, true)
                        .with(PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE, true)
                        .with(PdfToolbarProperties.ZOOM_CONTROLS_VISIBLE, true)
                        .with(PdfToolbarProperties.PAGE_NAV_AND_EDIT_VISIBLE, true)
                        .build();

        toolbar.setOnWidthChangedListener(this::onWidthChanged);

        // Set up the MCP to sync the Model and View
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, toolbar, PdfToolbarViewBinder::bind);
    }

    @Override
    public void onClick(View view) {
        int actionId = view.getId();
        float currentZoomFactor = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);

        if (actionId == R.id.zoom_increase_button) {
            mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, true));
        } else if (actionId == R.id.zoom_decrease_button) {
            mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, false));
        } else if (actionId == R.id.fit_to_page_button) {
            boolean showFitToHeight = mModel.get(PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON);
            mDelegate.toggleFitToPage(showFitToHeight, currentPageNumber - 1);
            mModel.set(PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON, !showFitToHeight);
        } else if (actionId == R.id.download_button) {
            mDelegate.download();
        } else if (actionId == R.id.rotate_button) {
            mDelegate.rotate();
        } else if (actionId == R.id.more_menu_button) {
            showMenu(view);
        }
    }

    private void toggleTwoPageView() {
        float currentZoomFactor = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);
        boolean isCurrentlyActive = mModel.get(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE);
        boolean newState = !isCurrentlyActive;
        mModel.set(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE, newState);
        mDelegate.toggleTwoPagesPerRow(newState, currentZoomFactor, currentPageNumber - 1);
    }

    private void showMenu(View anchorView) {
        ModelList modelList = new ModelList();

        if (!mModel.get(PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE)) {
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.pdf_download)
                            .withClickListener(
                                    v -> {
                                        mDelegate.download();
                                        dismissMenu();
                                    })
                            .build());
        }
        if (!mModel.get(PdfToolbarProperties.ROTATE_BUTTON_VISIBLE)) {
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.pdf_rotate)
                            .withClickListener(
                                    v -> {
                                        mDelegate.rotate();
                                        dismissMenu();
                                    })
                            .build());
        }
        if (!mModel.get(PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE)) {
            boolean showFitToHeight = mModel.get(PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON);
            int fitTitleRes = showFitToHeight ? R.string.pdf_fit_height : R.string.pdf_fit_width;
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(fitTitleRes)
                            .withClickListener(
                                    v -> {
                                        int currentPageNumber =
                                                mModel.get(
                                                        PdfToolbarProperties.CURRENT_PAGE_NUMBER);
                                        mDelegate.toggleFitToPage(
                                                showFitToHeight, currentPageNumber - 1);
                                        mModel.set(
                                                PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON,
                                                !showFitToHeight);
                                        dismissMenu();
                                    })
                            .build());
        }

        // Two-page view / Single page view item
        boolean isTwoPageActive = mModel.get(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE);
        int titleRes = isTwoPageActive ? R.string.pdf_single_page_view : R.string.pdf_two_page_view;
        ListItemBuilder twoPageItem =
                new ListItemBuilder()
                        .withTitleRes(titleRes)
                        .withClickListener(
                                v -> {
                                    toggleTwoPageView();
                                    dismissMenu();
                                });
        modelList.add(twoPageItem.build());

        // Document properties item
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.pdf_document_properties)
                        .withClickListener(
                                v -> {
                                    // TODO (crbug.com/479585910): Display document properties.
                                    dismissMenu();
                                })
                        .build());

        ListMenu.Delegate delegate =
                (model, view) -> {
                    View.OnClickListener listener =
                            model.get(ListMenuItemProperties.CLICK_LISTENER);
                    if (listener != null) {
                        listener.onClick(view);
                    }
                };

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        anchorView.getContext(), modelList, delegate);
        ListView listView = listMenu.getListView();
        if (listView != null) {
            listView.setItemsCanFocus(false);
            listView.setFocusable(false);
            listView.setFocusableInTouchMode(false);
        }

        View contentView = listMenu.getContentView();
        int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int widthPx = listMenu.getMaxItemWidth() + lateralPadding;

        mMenuWindow =
                new AnchoredPopupWindow.Builder(
                                anchorView.getContext(),
                                anchorView.getRootView(),
                                new ColorDrawable(android.graphics.Color.TRANSPARENT),
                                () -> contentView,
                                new ViewRectProvider(anchorView))
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setDismissOnTouchInteraction(true)
                        .setHorizontalOverlapAnchor(true)
                        .setVerticalOverlapAnchor(false)
                        .setDesiredContentWidth(widthPx)
                        .setAllowNonTouchableSize(true)
                        .build();

        mMenuWindow.show();
    }

    private void dismissMenu() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
            mMenuWindow = null;
        }
    }

    private float getNextZoomLevel(float currentZoomLevel, boolean increase) {
        int index = 0;
        // Find the first index where the zoom level is greater than or equal to current and move
        // to the next one if it exists.
        while (index < mZoomLevels.size() && mZoomLevels.get(index) <= currentZoomLevel) {
            index++;
        }

        if (increase) {
            // Return the next highest, or stay at the max if we're at the end
            return mZoomLevels.get(index);
        } else {
            // If the current zoom level is in the list, decrease by 1. Otherwise, decrease by 2.
            int targetIndex = mZoomLevels.contains(currentZoomLevel) ? index - 2 : index - 1;
            if (targetIndex < 0) targetIndex = 0;
            return mZoomLevels.get(targetIndex);
        }
    }

    /**
     * Called when the PDF document is successfully loaded.
     *
     * @param pageCount The total page count of the document.
     * @param title The title of the document.
     */
    public void onDocumentLoaded(int pageCount, String title) {
        mModel.set(PdfToolbarProperties.TOTAL_PAGE_COUNT, pageCount);
        mModel.set(PdfToolbarProperties.TITLE, title);
    }

    /**
     * Called when the viewport changes on the PDF viewer.
     *
     * @param firstVisiblePage The first visible page.
     * @param zoomLevel The current zoom level.
     */
    public void onViewportChanged(int firstVisiblePage, float zoomLevel) {
        // Fetch absolute state from engine as the single source of truth.
        // Keep the model 1-indexed.
        mModel.set(PdfToolbarProperties.CURRENT_PAGE_NUMBER, firstVisiblePage + 1);
        mModel.set(PdfToolbarProperties.ZOOM_LEVEL, zoomLevel);
        mModel.set(
                PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED, zoomLevel > mZoomLevels.get(0));
        mModel.set(
                PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED,
                zoomLevel < mZoomLevels.get(mZoomLevels.size() - 1));
    }

    private void onPageNumberSubmitted(int pageNumber) {
        int totalPageCount = mModel.get(PdfToolbarProperties.TOTAL_PAGE_COUNT);
        if (pageNumber >= 1 && pageNumber <= totalPageCount) {
            // Convert to 0-based index for the delegate.
            mDelegate.navigateToPage(pageNumber - 1);
        }
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN && event.isCtrlPressed()) {
            float currentZoomFactor = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);
            if (keyCode == KeyEvent.KEYCODE_EQUALS
                    || keyCode == KeyEvent.KEYCODE_PLUS
                    || keyCode == KeyEvent.KEYCODE_NUMPAD_ADD) {
                mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, true));
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MINUS
                    || keyCode == KeyEvent.KEYCODE_NUMPAD_SUBTRACT) {
                mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, false));
                return true;
            }
        }
        return false;
    }

    private void onWidthChanged(int widthPx) {
        if (mToolbar == null) return;
        float density = mToolbar.getResources().getDisplayMetrics().density;
        float widthDp = widthPx / density;

        mModel.set(
                PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE, widthDp > THRESHOLD_DOWNLOAD_DP);
        mModel.set(PdfToolbarProperties.ROTATE_BUTTON_VISIBLE, widthDp > THRESHOLD_ROTATE_DP);
        mModel.set(
                PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE, widthDp > THRESHOLD_FIT_DP);
        mModel.set(PdfToolbarProperties.ZOOM_CONTROLS_VISIBLE, widthDp > THRESHOLD_ZOOM_DP);
        mModel.set(PdfToolbarProperties.PAGE_NAV_AND_EDIT_VISIBLE, widthDp > THRESHOLD_NAV_EDIT_DP);
    }

    /** Destroys the coordinator and releases references held by the change processor. */
    public void destroy() {
        mPropertyModelChangeProcessor.destroy();
        dismissMenu();
        if (mToolbar != null) {
            mToolbar.setOnWidthChangedListener(null);
        }
        mToolbar = null;
    }
}
