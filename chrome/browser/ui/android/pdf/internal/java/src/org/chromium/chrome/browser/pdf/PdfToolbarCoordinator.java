// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewStub;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;

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
public class PdfToolbarCoordinator implements View.OnClickListener {
    private static final float THRESHOLD_DOWNLOAD_DP = 800f;

    private static final float THRESHOLD_FIT_DP = 700f;
    private static final float THRESHOLD_ZOOM_DP = 650f;
    private static final float THRESHOLD_NAV_EDIT_DP = 600f;
    @VisibleForTesting static final float ZOOM_EPSILON = 0.005f;

    private final PropertyModel mModel;
    private final PdfToolbarActionsDelegate mDelegate;
    private final PropertyModelChangeProcessor<PropertyModel, PdfToolbar, PropertyKey>
            mPropertyModelChangeProcessor;
    private final List<Float> mDisplayZoomLevels =
            List.of(
                    0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f,
                    2.0f, 2.5f, 3.0f, 4.0f, 5.0f);
    private float mDefaultZoomLevel = -1f;
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
                        .with(PdfToolbarProperties.CURRENT_PAGE_NUMBER, 1)
                        .with(PdfToolbarProperties.ZOOM_LEVEL, 1.0f)
                        .with(PdfToolbarProperties.SHOW_FIT_TO_PAGE_ICON, true)
                        .with(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE, false)
                        .with(
                                PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE,
                                PdfUtils.isInlinePdfV2DownloadEnabled())
                        .with(PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE, true)
                        .with(PdfToolbarProperties.ZOOM_CONTROLS_VISIBLE, true)
                        .with(PdfToolbarProperties.PAGE_NAV_AND_EDIT_VISIBLE, true)
                        .with(PdfToolbarProperties.DONE_BUTTON_VISIBLE, false)
                        .build();

        toolbar.setOnWidthChangedListener(this::onWidthChanged);

        // Set up the MCP to sync the Model and View
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, toolbar, PdfToolbarViewBinder::bind);
    }

    @Override
    public void onClick(View view) {
        int actionId = view.getId();
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);

        if (actionId == R.id.zoom_increase_button) {
            Float nextZoomLevel = getNextEngineZoomLevel(/* increase= */ true);
            if (nextZoomLevel != null) {
                PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.ZOOM_IN);
                mDelegate.changeZoomLevel(nextZoomLevel);
            }
        } else if (actionId == R.id.zoom_decrease_button) {
            Float nextZoomLevel = getNextEngineZoomLevel(/* increase= */ false);
            if (nextZoomLevel != null) {
                PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.ZOOM_OUT);
                mDelegate.changeZoomLevel(nextZoomLevel);
            }
        } else if (actionId == R.id.fit_to_page_button) {
            boolean showFitToPage = mModel.get(PdfToolbarProperties.SHOW_FIT_TO_PAGE_ICON);
            PdfUtils.recordToolbarAction(
                    showFitToPage
                            ? PdfUtils.PdfToolbarAction.FIT_TO_PAGE
                            : PdfUtils.PdfToolbarAction.FIT_TO_WIDTH);
            mDelegate.toggleFitToPage(showFitToPage, Math.max(0, currentPageNumber - 1));
            mModel.set(PdfToolbarProperties.SHOW_FIT_TO_PAGE_ICON, !showFitToPage);
        } else if (actionId == R.id.download_button) {
            mDelegate.download();

        } else if (actionId == R.id.more_menu_button) {
            showMenu(view);
        } else if (actionId == R.id.print_button) {
            PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.PRINT);
            mDelegate.print();
        } else if (actionId == R.id.done_button) {
            mDelegate.setEditMode(false);
        } else if (actionId == R.id.edit_button) {
            if (!mModel.get(PdfToolbarProperties.EDIT_MODE_ACTIVE)) {
                mDelegate.setEditMode(true);
            }
        }
    }

    private void toggleTwoPageView() {
        float currentZoomFactor = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);
        boolean isCurrentlyActive = mModel.get(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE);
        boolean newState = !isCurrentlyActive;
        if (newState) {
            PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.TWO_PAGE_VIEW);
        } else {
            PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.SINGLE_PAGE_VIEW);
        }
        mModel.set(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE, newState);
        mDelegate.toggleTwoPagesPerRow(
                newState,
                getEngineZoomLevel(currentZoomFactor),
                Math.max(0, currentPageNumber - 1));
    }

    /**
     * Resets TWO_PAGES_PER_ROW_ACTIVE to false.
     *
     * <p>Unlike ZOOM_LEVEL (automatically reset via PdfView's viewport change listener) and
     * EDIT_MODE_ACTIVE (automatically reset via EditablePdfViewerFragment's edit mode callbacks),
     * AndroidX PdfView does not provide a callback when the pages-per-row state changes or resets.
     * Since PdfView defaults to single-page view when a document is loaded, reloaded, or reset,
     * two-pages-per-row uniquely requires manual resets in all these places to keep the toolbar
     * state in sync.
     */
    void resetTwoPagesPerRow() {
        mModel.set(PdfToolbarProperties.TWO_PAGES_PER_ROW_ACTIVE, false);
    }

    private void showMenu(View anchorView) {
        ModelList modelList = new ModelList();

        if (PdfUtils.isInlinePdfV2DownloadEnabled()
                && !mModel.get(PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE)) {
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

        if (!mModel.get(PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE)) {
            boolean showFitToPage = mModel.get(PdfToolbarProperties.SHOW_FIT_TO_PAGE_ICON);
            int fitTitleRes = showFitToPage ? R.string.pdf_fit_page : R.string.pdf_fit_width;
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(fitTitleRes)
                            .withClickListener(
                                    v -> {
                                        int currentPageNumber =
                                                mModel.get(
                                                        PdfToolbarProperties.CURRENT_PAGE_NUMBER);
                                        PdfUtils.recordToolbarAction(
                                                showFitToPage
                                                        ? PdfUtils.PdfToolbarAction.FIT_TO_PAGE
                                                        : PdfUtils.PdfToolbarAction.FIT_TO_WIDTH);
                                        mDelegate.toggleFitToPage(
                                                showFitToPage, Math.max(0, currentPageNumber - 1));
                                        mModel.set(
                                                PdfToolbarProperties.SHOW_FIT_TO_PAGE_ICON,
                                                !showFitToPage);
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
        if (mModel.get(PdfToolbarProperties.TOTAL_PAGE_COUNT) > 0) {
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.pdf_document_properties)
                            .withClickListener(
                                    v -> {
                                        PdfUtils.recordToolbarAction(
                                                PdfUtils.PdfToolbarAction.DOCUMENT_PROPERTIES);
                                        mDelegate.showDocumentProperties();
                                        dismissMenu();
                                    })
                            .build());
        }

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

    /**
     * Sets the default engine zoom level used to normalize display zoom levels.
     *
     * @param defaultZoomLevel The raw engine zoom level corresponding to 100% display zoom.
     */
    public void setDefaultZoomLevel(float defaultZoomLevel) {
        if (defaultZoomLevel > 0f) {
            mDefaultZoomLevel = defaultZoomLevel;
        }
    }

    /**
     * Returns the next engine zoom level based on the current display zoom level and the direction
     * of the zoom change.
     *
     * @param increase Whether to increase the zoom level.
     * @return The next engine zoom level, or null if the zoom level cannot be changed.
     */
    public @Nullable Float getNextEngineZoomLevel(boolean increase) {
        float currentDisplayZoom = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);

        if (increase) {
            if (currentDisplayZoom >= mDisplayZoomLevels.get(mDisplayZoomLevels.size() - 1)) {
                // Already at max zoom, return null to indicate no change.
                return null;
            }
            // Find the smallest value strictly greater than currentDisplayZoom
            for (float level : mDisplayZoomLevels) {
                if (level > currentDisplayZoom + ZOOM_EPSILON) {
                    return getEngineZoomLevel(level);
                }
            }
            // currentDisplayZoom is slightly below max zoom, so just return max zoom.
            return getEngineZoomLevel(mDisplayZoomLevels.get(mDisplayZoomLevels.size() - 1));

        } else {
            if (currentDisplayZoom <= mDisplayZoomLevels.get(0)) {
                // Already at min zoom, return null to indicate no change.
                return null;
            }
            // Find the largest value strictly smaller than currentDisplayZoom
            for (int i = mDisplayZoomLevels.size() - 1; i >= 0; i--) {
                float level = mDisplayZoomLevels.get(i);
                if (level < currentDisplayZoom - ZOOM_EPSILON) {
                    return getEngineZoomLevel(level);
                }
            }
            // currentDisplayZoom is slightly above min zoom, so just return min zoom.
            return getEngineZoomLevel(mDisplayZoomLevels.get(0));
        }
    }

    /**
     * Converts the raw engine zoom level to the normalized display zoom level, where the
     * initial/default zoom level is represented as 1.0f (100%).
     */
    private float getDisplayZoomLevel(float engineZoomLevel) {
        if (mDefaultZoomLevel <= 0f) return engineZoomLevel;
        return engineZoomLevel / mDefaultZoomLevel;
    }

    /** Converts a normalized display zoom level back to the raw engine zoom level. */
    private float getEngineZoomLevel(float displayZoomLevel) {
        if (mDefaultZoomLevel <= 0f) return displayZoomLevel;
        return displayZoomLevel * mDefaultZoomLevel;
    }

    float getDefaultZoomLevel() {
        return mDefaultZoomLevel;
    }

    /**
     * Called when the PDF document is successfully loaded.
     *
     * @param pageCount The total page count of the document.
     * @param title The title of the document.
     */
    public void onDocumentLoaded(int pageCount, String title) {
        mDefaultZoomLevel = -1f;
        mModel.set(PdfToolbarProperties.TOTAL_PAGE_COUNT, pageCount);
        mModel.set(PdfToolbarProperties.TITLE, title);
        // Manually reset two-pages-per-row state since PdfView defaults to single-page view
        // on load and does not provide a callback when pages-per-row resets.
        resetTwoPagesPerRow();
    }

    /**
     * Called when the viewport changes on the PDF viewer.
     *
     * @param firstVisiblePage The first visible page.
     * @param zoomLevel The current zoom level.
     */
    public void onViewportChanged(int firstVisiblePage, float zoomLevel) {
        float displayZoomLevel = getDisplayZoomLevel(zoomLevel);
        // Fetch absolute state from engine as the single source of truth.
        // Keep the model 1-indexed.
        float minZoom = mDisplayZoomLevels.get(0);
        float maxZoom = mDisplayZoomLevels.get(mDisplayZoomLevels.size() - 1);
        mModel.set(PdfToolbarProperties.CURRENT_PAGE_NUMBER, firstVisiblePage + 1);
        mModel.set(PdfToolbarProperties.ZOOM_LEVEL, displayZoomLevel);
        mModel.set(
                PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED,
                displayZoomLevel > minZoom + ZOOM_EPSILON);
        mModel.set(
                PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED,
                displayZoomLevel < maxZoom - ZOOM_EPSILON);
    }

    private void onPageNumberSubmitted(int pageNumber) {
        int totalPageCount = mModel.get(PdfToolbarProperties.TOTAL_PAGE_COUNT);
        if (pageNumber >= 1 && pageNumber <= totalPageCount) {
            PdfUtils.recordToolbarAction(PdfUtils.PdfToolbarAction.PAGE_NAVIGATION);
            // Convert to 0-based index for the delegate.
            mDelegate.navigateToPage(pageNumber - 1);
        }
    }

    private void onWidthChanged(int widthPx) {
        if (mToolbar == null) return;
        float density = mToolbar.getResources().getDisplayMetrics().density;
        float widthDp = widthPx / density;

        boolean showNavEdit = widthDp > THRESHOLD_NAV_EDIT_DP;
        mModel.set(
                PdfToolbarProperties.DOWNLOAD_BUTTON_VISIBLE,
                PdfUtils.isInlinePdfV2DownloadEnabled() && widthDp > THRESHOLD_DOWNLOAD_DP);

        mModel.set(PdfToolbarProperties.FIT_TO_PAGE_BUTTON_VISIBLE, widthDp > THRESHOLD_FIT_DP);
        mModel.set(PdfToolbarProperties.ZOOM_CONTROLS_VISIBLE, widthDp > THRESHOLD_ZOOM_DP);
        mModel.set(PdfToolbarProperties.PAGE_NAV_AND_EDIT_VISIBLE, showNavEdit);
        updateDoneButtonVisibility();
        mDelegate.onPageNavAndEditVisibilityChanged(showNavEdit);
    }


    /** Sets whether edit mode is active in the model. */
    public void setEditModeActive(boolean active) {
        mModel.set(PdfToolbarProperties.EDIT_MODE_ACTIVE, active);
        updateDoneButtonVisibility();
    }

    private void updateDoneButtonVisibility() {
        boolean editMode = mModel.get(PdfToolbarProperties.EDIT_MODE_ACTIVE);
        mModel.set(PdfToolbarProperties.DONE_BUTTON_VISIBLE, editMode);
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
