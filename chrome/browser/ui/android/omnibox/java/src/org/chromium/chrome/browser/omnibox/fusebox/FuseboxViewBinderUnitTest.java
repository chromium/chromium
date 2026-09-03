// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.BackgroundStyle;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxViewHolder.AnchoringMode;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Unit tests for {@link FuseboxViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxViewBinderUnitTest {
    @IntDef({
        Variant.DEFAULT,
        Variant.COMPACT,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface Variant {
        int DEFAULT = 0;
        int COMPACT = 1;
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private AnchoredPopupWindow mPopupWindow;
    @Mock private DynamicRectProvider mDynamicRectProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Runnable mRunnable;
    @Mock private SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;

    private final PropertyModel mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

    private ActivityController<TestActivity> mActivityController;
    private FuseboxViewHolder mViewHolder;
    private FuseboxPopup mPopup;
    private FuseboxViewBinder mBinder;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();

        // Initialize location bar layout.
        ConstraintLayout parent = new ConstraintLayout(activity);
        LayoutInflater.from(activity).inflate(R.layout.location_bar, parent);

        ViewGroup popupView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.fusebox_context_popup, /* root= */ null);
        lenient().doReturn(popupView).when(mPopupWindow).getContentView();

        lenient().doReturn(null).when(mWindowAndroid).getInsetObserver();
        mPopup =
                new FuseboxPopup(
                        activity,
                        mWindowAndroid,
                        mPopupWindow,
                        popupView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ false);
        mViewHolder = new FuseboxViewHolder(parent, mPopup);

        // Initialize workable defaults.
        mModel.set(FuseboxProperties.PLUS_BUTTON_VISIBLE, true);
        mModel.set(FuseboxProperties.FUSEBOX_STATE, FuseboxState.EXPANDED);
        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT, "test label");
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE, false);
        mModel.set(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);
        mModel.set(
                FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE,
                BackgroundStyle.INTERACT_ONLY_SMALL);

        var resourceProvider =
                new OmniboxResourceProvider(activity, BrandedColorScheme.APP_DEFAULT);
        mBinder = new FuseboxViewBinder(resourceProvider);
        PropertyModelChangeProcessor.create(mModel, mViewHolder, mBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    private View getDynamicButton(FuseboxPopup popup, int index) {
        ViewGroup group = popup.mViewGroup;
        int headerIndex = group.indexOfChild(popup.mModelsHeader);
        return group.getChildAt(headerIndex + 1 + index);
    }

    private View getDynamicButton(int index) {
        return getDynamicButton(mPopup, index);
    }

    private View getDynamicToolButton(int index) {
        ViewGroup group = mPopup.mViewGroup;
        int headerIndex = group.indexOfChild(mPopup.mToolsHeader);
        return group.getChildAt(headerIndex + 1 + index);
    }

    private FuseboxViewHolder createBottomSheetViewHolder() {
        Activity activity = mActivityController.get();
        ViewGroup popupView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.fusebox_context_popup, /* root= */ null);
        FuseboxPopup popup =
                new FuseboxPopup(
                        activity,
                        mWindowAndroid,
                        mPopupWindow,
                        popupView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ true);
        return new FuseboxViewHolder(mViewHolder.parentView, popup);
    }

    private PropertyModel createBottomSheetModel() {
        return new PropertyModel.Builder(FuseboxProperties.ALL_KEYS)
                .with(FuseboxProperties.POPUP_IS_BOTTOM_SHEET, true)
                .with(FuseboxProperties.PLUS_BUTTON_VISIBLE, true)
                .with(FuseboxProperties.FUSEBOX_STATE, FuseboxState.EXPANDED)
                .with(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.SEARCH)
                .with(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT, "test label")
                .with(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE, false)
                .with(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT)
                .with(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR)
                .with(
                        FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE,
                        BackgroundStyle.INTERACT_ONLY_SMALL)
                .build();
    }

    private void addModelButton(PropertyModel model, FuseboxViewHolder viewHolder) {
        PopupButtonData buttonData =
                new PopupButtonDataBuilder().withIconId(IconResourceIds.AUTORENEW_VALUE).build();
        model.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(buttonData));
        if (viewHolder != mViewHolder) {
            mBinder.bind(model, viewHolder, FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        }
    }

    private void addModelButton(FuseboxViewHolder viewHolder) {
        addModelButton(mModel, viewHolder);
    }

    private void addModelButton() {
        addModelButton(mViewHolder);
    }

    private void configureFusebox(@Variant int testCase, @AutocompleteRequestType int requestType) {
        // Reflect the active state of the fusebox toolbar.
        mModel.set(
                FuseboxProperties.FUSEBOX_STATE,
                testCase == Variant.COMPACT ? FuseboxState.COMPACT : FuseboxState.EXPANDED);
        mModel.set(FuseboxProperties.REQUEST_TYPE, requestType);
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT, "test label");
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE, false);
    }

    @Test
    public void plusButtonVisible_setsVisibility() {
        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        mModel.set(FuseboxProperties.PLUS_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.plusButton.getVisibility());

        mModel.set(FuseboxProperties.PLUS_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mViewHolder.plusButton.getVisibility());
    }

    @Test
    public void attachmentsVisible_setsVisibilityAndTogglesSwitch() {
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.attachmentsView.getVisibility());

        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, false);
        assertEquals(View.GONE, mViewHolder.attachmentsView.getVisibility());
    }

    @Test
    public void adapter_isSet() {
        mModel.set(FuseboxProperties.ADAPTER, mSimpleRecyclerViewAdapter);
        assertEquals(mSimpleRecyclerViewAdapter, mViewHolder.attachmentsView.getAdapter());
    }

    @Test
    public void plusButtonClickListener_isCalled() {
        mModel.set(FuseboxProperties.PLUS_BUTTON_CLICKED, mRunnable);

        mViewHolder.plusButton.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void cameraButtonClickListener_isCalled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED, mRunnable);

        mPopup.mCameraButton.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void galleryButtonClickListener_isCalled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED, mRunnable);

        mPopup.mGalleryButton.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void fileButtonClickListener_isCalled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED, mRunnable);

        mPopup.mFileButton.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void tabPickerButtonClickListener_isCalled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED, mRunnable);

        mPopup.mTabButton.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void requestTypeButtonClicked_setsListener() {
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED, mRunnable);
        mViewHolder.requestType.performClick();
        verify(mRunnable).run();
    }

    @Test
    public void updateButtonsVisibility_AndStyling_noParams() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        assertEquals(View.GONE, mViewHolder.requestType.getVisibility());
    }

    @Test
    public void updateRequestTypeButton_nonAimRequest_doesNotShowButton() {
        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        mModel.set(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE, true);
        assertEquals(View.GONE, mViewHolder.requestType.getVisibility());

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
    }

    @Test
    public void reanchorViewsForCompactFusebox_compactModeSearch() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.plusButton.getLayoutParams();
        assertEquals(R.id.url_bar, lp.topToTop);
        assertEquals(ConstraintSet.UNSET, lp.topToBottom);
        assertEquals(ConstraintSet.UNSET, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_notCompactMode() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.plusButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.url_bar, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_popoverLayoutMode() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.REANCHOR_VIEWS_DURATION_HISTOGRAM);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        assertEquals(AnchoringMode.POPOVER, mViewHolder.currentAnchoringMode);
        var lp = (ConstraintLayout.LayoutParams) mViewHolder.plusButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.omnibox_suggestions_dropdown, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
        histogramWatcher.assertExpected();
    }

    @Test
    public void reanchorViewsForCompactFusebox_deduplicatesWhenOptimizationsEnabled() {
        OmniboxFeatures.sModelPickerOptimizations.setForTesting(/* overrideValue= */ true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.REANCHOR_VIEWS_DURATION_HISTOGRAM);
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);

        assertEquals(AnchoringMode.TOOLBAR_SINGLE_LINE, mViewHolder.currentAnchoringMode);
        var lp = (ConstraintLayout.LayoutParams) mViewHolder.plusButton.getLayoutParams();
        assertEquals(R.id.url_bar, lp.topToTop);
        assertEquals(ConstraintSet.UNSET, lp.topToBottom);
        assertEquals(ConstraintSet.UNSET, lp.bottomToBottom);
        histogramWatcher.assertExpected();

        // Transitioning between DISABLED and COMPACT maintains singleLine without re-anchoring.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.REANCHOR_VIEWS_DURATION_HISTOGRAM);
        mModel.set(FuseboxProperties.FUSEBOX_STATE, FuseboxState.DISABLED);
        assertEquals(AnchoringMode.TOOLBAR_SINGLE_LINE, mViewHolder.currentAnchoringMode);
        var lpDisabled = (ConstraintLayout.LayoutParams) mViewHolder.plusButton.getLayoutParams();
        assertEquals(R.id.url_bar, lpDisabled.topToTop);
        assertEquals(ConstraintSet.UNSET, lpDisabled.topToBottom);
        assertEquals(ConstraintSet.UNSET, lpDisabled.bottomToBottom);
        histogramWatcher.assertExpected();

        // Transitioning to EXPANDED updates anchoring mode to TOOLBAR_MULTI_LINE.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.REANCHOR_VIEWS_DURATION_HISTOGRAM);
        mModel.set(FuseboxProperties.FUSEBOX_STATE, FuseboxState.EXPANDED);
        assertEquals(AnchoringMode.TOOLBAR_MULTI_LINE, mViewHolder.currentAnchoringMode);
        histogramWatcher.assertExpected();
    }

    @Test
    public void cameraButtonVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mCameraButton.getVisibility());

        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mCameraButton.getVisibility());
    }

    @Test
    public void galleryButtonVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mGalleryButton.getVisibility());

        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mGalleryButton.getVisibility());
    }

    @Test
    public void fileButtonVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mFileButton.getVisibility());

        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mFileButton.getVisibility());
    }

    @Test
    public void addCurrentTabButton() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mAddCurrentTab.getVisibility());
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mAddCurrentTab.getVisibility());

        assertNull(((ImageView) mPopup.mAddCurrentTab.findViewById(R.id.start_icon)).getDrawable());

        Bitmap favicon = UiUtils.createBitmap(/* size= */ 1, Color.RED);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON, favicon);
        Drawable faviconDrawable =
                ((ImageView) mPopup.mAddCurrentTab.findViewById(R.id.start_icon)).getDrawable();
        assertNotNull(faviconDrawable);

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON, null);
        Drawable fallbackDrawable =
                ((ImageView) mPopup.mAddCurrentTab.findViewById(R.id.start_icon)).getDrawable();
        assertNotNull(fallbackDrawable);
        assertNotEquals(fallbackDrawable, faviconDrawable);
    }

    @Test
    public void testCurrentTabButtonEnabled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, true);
        assertTrue(mViewHolder.popup.mAddCurrentTab.isEnabled());
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, false);
        assertFalse(mViewHolder.popup.mAddCurrentTab.isEnabled());
    }

    @Test
    public void testCurrentTabButtonEnabled_withFavicon() {
        Bitmap favicon = UiUtils.createBitmap(/* size= */ 1, Color.RED);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON, favicon);

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, true);
        assertTrue(mViewHolder.popup.mAddCurrentTab.isEnabled());

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, false);
        assertFalse(mViewHolder.popup.mAddCurrentTab.isEnabled());
    }

    @Test
    public void requestTypeDrawable() {
        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.IMAGE_GENERATION);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.DEEP_SEARCH);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.CANVAS);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
    }

    @Test
    public void modelButtonClickListener_isCalled() {
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withOnClicked(mRunnable).build()));

        getDynamicButton(0).performClick();
        verify(mRunnable).run();
    }

    @Test
    public void headersText_setsText() {
        mModel.set(FuseboxProperties.POPUP_TOOL_HEADER_TEXT, "Custom Tool Header");
        assertEquals("Custom Tool Header", mPopup.mToolsHeader.getText());

        mModel.set(FuseboxProperties.POPUP_MODEL_HEADER_TEXT, "Custom Model Header");
        assertEquals("Custom Model Header", mPopup.mModelsHeader.getText());
    }

    @Test
    public void dividersAndHeadersVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mToolsDivider.getVisibility());
        mModel.set(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mToolsDivider.getVisibility());

        mModel.set(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mToolsHeader.getVisibility());
        mModel.set(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mToolsHeader.getVisibility());

        mModel.set(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mModelsDivider.getVisibility());
        mModel.set(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mModelsDivider.getVisibility());

        mModel.set(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mModelsHeader.getVisibility());
        mModel.set(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mModelsHeader.getVisibility());
    }

    @Test
    public void modelButtonEnabled_setsEnabled() {
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withEnabled(/* enabled= */ true).build()));
        assertTrue(getDynamicButton(0).isEnabled());

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withEnabled(/* enabled= */ false).build()));
        assertFalse(getDynamicButton(0).isEnabled());
    }

    @Test
    public void modelButtonA11y_setsContentDescription() {
        Resources res = mActivityController.get().getResources();
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(
                        new PopupButtonDataBuilder()
                                .withText("custom model")
                                .withType(PopupButtonType.MODEL)
                                .withSelected(/* selected= */ true)
                                .build()));
        assertEquals(
                res.getString(R.string.acc_fusebox_popup_button_selected, "custom model"),
                getDynamicButton(0).getContentDescription());

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(
                        new PopupButtonDataBuilder()
                                .withText("custom model")
                                .withType(PopupButtonType.MODEL)
                                .withSelected(/* selected= */ false)
                                .build()));
        assertEquals("custom model", getDynamicButton(0).getContentDescription());
    }

    @Test
    public void sendButtonA11y_setsContentDescription() {
        var res = mActivityController.get().getResources();

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertEquals(
                res.getString(R.string.acc_send_button_send_to_ai),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(
                res.getString(R.string.acc_send_button_create_image),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.DEEP_SEARCH);
        assertEquals(
                res.getString(R.string.ntp_compose_deep_search),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.CANVAS);
        assertEquals(
                res.getString(R.string.ntp_compose_canvas),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        assertEquals(
                res.getString(R.string.acc_send_button_search_or_navigate),
                mViewHolder.navigateButton.getContentDescription());
    }

    @Test
    public void modelSelectionDrawables() {
        PopupButtonData selectedData =
                new PopupButtonDataBuilder().withSelected(/* selected= */ true).build();
        PopupButtonData notSelectedData =
                new PopupButtonDataBuilder().withSelected(/* selected= */ false).build();
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(notSelectedData, notSelectedData));
        assertEndIconSelected(getDynamicButton(0), /* selected= */ false);
        assertEndIconSelected(getDynamicButton(1), /* selected= */ false);

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(selectedData, notSelectedData));
        assertEndIconSelected(getDynamicButton(0), /* selected= */ true);
        assertEndIconSelected(getDynamicButton(1), /* selected= */ false);

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(notSelectedData, selectedData));
        assertEndIconSelected(getDynamicButton(0), /* selected= */ false);
        assertEndIconSelected(getDynamicButton(1), /* selected= */ true);
    }

    @Test
    public void modelButtonText_setsText() {
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withText("custom text").build()));
        View buttonView = getDynamicButton(0);
        TextView textView = buttonView.findViewById(R.id.action_text);
        assertEquals("custom text", textView.getText());
    }

    @Test
    public void modelButtonIcon_setsIcon() {
        PopupButtonData buttonData =
                new PopupButtonDataBuilder().withIconId(IconResourceIds.AUTORENEW_VALUE).build();
        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(buttonData));
        assertNotNull(
                ((ImageView) getDynamicButton(0).findViewById(R.id.start_icon)).getDrawable());
    }

    @Test
    public void modelButtonCount_removesExcessButtons() {
        PopupButtonData data1 = new PopupButtonDataBuilder().withText("button 1").build();
        PopupButtonData data2 = new PopupButtonDataBuilder().withText("button 2").build();

        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(data1, data2));
        int headerIndex = mPopup.mViewGroup.indexOfChild(mPopup.mModelsHeader);
        assertEquals(2, mPopup.mViewGroup.getChildCount() - (headerIndex + 1));
        assertEquals(5, mPopup.mAttachmentButtons.size());
        assertEquals(2, mPopup.mDynamicThemedButtons.size());

        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(data1));
        assertEquals(1, mPopup.mViewGroup.getChildCount() - (headerIndex + 1));
        assertEquals(5, mPopup.mAttachmentButtons.size());
        assertEquals(1, mPopup.mDynamicThemedButtons.size());
    }

    @Test
    public void toolButtonCount_removesExcessButtons() {
        PopupButtonData data1 =
                new PopupButtonDataBuilder()
                        .withText("tool 1")
                        .withType(PopupButtonType.TOOL)
                        .build();
        PopupButtonData data2 =
                new PopupButtonDataBuilder()
                        .withText("tool 2")
                        .withType(PopupButtonType.TOOL)
                        .build();

        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, List.of(data1, data2));
        int headerIndex = mPopup.mViewGroup.indexOfChild(mPopup.mToolsHeader);
        int dividerIndex = mPopup.mViewGroup.indexOfChild(mPopup.mModelsDivider);
        assertEquals(2, dividerIndex - (headerIndex + 1));

        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, List.of(data1));
        dividerIndex = mPopup.mViewGroup.indexOfChild(mPopup.mModelsDivider);
        assertEquals(1, dividerIndex - (headerIndex + 1));
    }

    @Test
    public void toolButtonText_setsText() {
        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(
                        new PopupButtonDataBuilder()
                                .withText("custom tool text")
                                .withType(PopupButtonType.TOOL)
                                .build()));
        View buttonView = getDynamicToolButton(0);
        TextView textView = buttonView.findViewById(R.id.action_text);
        assertEquals("custom tool text", textView.getText());
    }

    @Test
    public void toolButtonIcon_setsIcon() {
        PopupButtonData buttonData =
                new PopupButtonDataBuilder()
                        .withIconId(IconResourceIds.BANANA_VALUE)
                        .withType(PopupButtonType.TOOL)
                        .build();
        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, List.of(buttonData));
        assertNotNull(
                ((ImageView) getDynamicToolButton(0).findViewById(R.id.start_icon)).getDrawable());
    }

    @Test
    public void modelButtonIcon_acute_setsIcon() {
        PopupButtonData buttonData =
                new PopupButtonDataBuilder()
                        .withIconId(IconResourceIds.ACUTE_VALUE)
                        .withType(PopupButtonType.MODEL)
                        .build();
        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(buttonData));
        assertNotNull(
                ((ImageView) getDynamicButton(0).findViewById(R.id.start_icon)).getDrawable());
    }

    @Test
    public void toolSelectionDrawables() {
        PopupButtonData selectedData =
                new PopupButtonDataBuilder()
                        .withSelected(/* selected= */ true)
                        .withType(PopupButtonType.TOOL)
                        .build();
        PopupButtonData notSelectedData =
                new PopupButtonDataBuilder()
                        .withSelected(/* selected= */ false)
                        .withType(PopupButtonType.TOOL)
                        .build();
        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(notSelectedData, notSelectedData));
        assertEndIconSelected(getDynamicToolButton(0), /* selected= */ false);
        assertEndIconSelected(getDynamicToolButton(1), /* selected= */ false);

        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(selectedData, notSelectedData));
        assertEndIconSelected(getDynamicToolButton(0), /* selected= */ true);
        assertEndIconSelected(getDynamicToolButton(1), /* selected= */ false);
    }

    @Test
    public void toolButtonA11y_setsContentDescription() {
        Resources res = mActivityController.get().getResources();
        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(
                        new PopupButtonDataBuilder()
                                .withText("custom tool")
                                .withType(PopupButtonType.TOOL)
                                .withSelected(/* selected= */ true)
                                .build()));
        assertEquals(
                res.getString(R.string.acc_fusebox_popup_button_selected, "custom tool"),
                getDynamicToolButton(0).getContentDescription());

        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(
                        new PopupButtonDataBuilder()
                                .withText("custom tool")
                                .withType(PopupButtonType.TOOL)
                                .withSelected(/* selected= */ false)
                                .build()));
        assertEquals("custom tool", getDynamicToolButton(0).getContentDescription());
    }

    @Test
    public void recentTabsDividersAndHeadersVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mRecentTabsDivider.getVisibility());
        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mRecentTabsDivider.getVisibility());

        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mRecentTabsHeader.getVisibility());
        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mRecentTabsHeader.getVisibility());
    }

    @Test
    public void recentTabsCount_removesExcessButtons() {
        PopupButtonData data1 =
                new PopupButtonDataBuilder()
                        .withText("tab 1")
                        .withType(PopupButtonType.RECENT_TAB)
                        .build();
        PopupButtonData data2 =
                new PopupButtonDataBuilder()
                        .withText("tab 2")
                        .withType(PopupButtonType.RECENT_TAB)
                        .build();

        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST, List.of(data1, data2));
        assertEquals(2, mPopup.mRecentTabsContainer.getChildCount());

        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST, List.of(data1));
        assertEquals(1, mPopup.mRecentTabsContainer.getChildCount());
    }

    @Test
    public void recentTabsBinding_truncationAndFavicon() {
        Bitmap favicon = UiUtils.createBitmap(/* size= */ 1, Color.BLUE);
        PopupButtonData data =
                new PopupButtonDataBuilder()
                        .withText("very long tab title")
                        .withType(PopupButtonType.RECENT_TAB)
                        .withCustomIcon(favicon)
                        .build();

        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST, List.of(data));
        View buttonView = mPopup.mRecentTabsContainer.getChildAt(0);
        TextView textView = buttonView.findViewById(R.id.action_text);
        ImageView imageView = buttonView.findViewById(R.id.start_icon);

        assertEquals("very long tab title", textView.getText());
        assertEquals(1, textView.getMaxLines());
        assertEquals(TextUtils.TruncateAt.END, textView.getEllipsize());
        assertNotNull(imageView.getDrawable());
    }

    @Test
    public void recentTabsEnabled_withFavicon() {
        Bitmap favicon = UiUtils.createBitmap(/* size= */ 1, Color.BLUE);
        PopupButtonData dataWithFavicon =
                new PopupButtonDataBuilder()
                        .withText("tab with favicon")
                        .withType(PopupButtonType.RECENT_TAB)
                        .withCustomIcon(favicon)
                        .build();
        PopupButtonData dataWithoutFavicon =
                new PopupButtonDataBuilder()
                        .withText("tab without favicon")
                        .withType(PopupButtonType.RECENT_TAB)
                        .build();
        mModel.set(
                FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST,
                List.of(dataWithFavicon, dataWithoutFavicon));

        View childWithFavicon = mPopup.mRecentTabsContainer.getChildAt(0);
        View childWithoutFavicon = mPopup.mRecentTabsContainer.getChildAt(1);
        ImageView imageWithFavicon = childWithFavicon.findViewById(R.id.start_icon);
        ImageView imageWithoutFavicon = childWithoutFavicon.findViewById(R.id.start_icon);

        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_ENABLED, true);
        assertTrue(mPopup.mRecentTabsContainer.getChildAt(0).isEnabled());
        assertTrue(mPopup.mRecentTabsContainer.getChildAt(1).isEnabled());
        assertNotNull(imageWithFavicon.getDrawable().getColorFilter());
        assertNull(imageWithoutFavicon.getDrawable().getColorFilter());

        // Toggle disabled:
        mModel.set(FuseboxProperties.POPUP_RECENT_TABS_ENABLED, false);
        assertFalse(childWithFavicon.isEnabled());
        assertFalse(childWithoutFavicon.isEnabled());
        assertNotNull(imageWithFavicon.getDrawable().getColorFilter());
        assertNull(imageWithoutFavicon.getDrawable().getColorFilter());
    }

    private static class PopupButtonDataBuilder {
        private Runnable mOnClicked = CallbackUtils.emptyRunnable();
        private String mText = "test";
        private int mIconId;
        private boolean mEnabled = true;
        private boolean mSelected;
        private @PopupButtonType int mType = PopupButtonType.MODEL;
        private @Nullable Bitmap mCustomIcon;

        PopupButtonDataBuilder withOnClicked(Runnable onClicked) {
            mOnClicked = onClicked;
            return this;
        }

        PopupButtonDataBuilder withType(@PopupButtonType int type) {
            mType = type;
            return this;
        }

        PopupButtonDataBuilder withText(String text) {
            mText = text;
            return this;
        }

        PopupButtonDataBuilder withIconId(int iconId) {
            mIconId = iconId;
            return this;
        }

        PopupButtonDataBuilder withEnabled(boolean enabled) {
            mEnabled = enabled;
            return this;
        }

        PopupButtonDataBuilder withSelected(boolean selected) {
            mSelected = selected;
            return this;
        }

        PopupButtonDataBuilder withCustomIcon(@Nullable Bitmap customIcon) {
            mCustomIcon = customIcon;
            return this;
        }

        PopupButtonData build() {
            if (mType == PopupButtonType.RECENT_TAB) {
                return new PopupButtonData(
                        (data) -> mOnClicked.run(),
                        mText,
                        mCustomIcon,
                        /* enabled= */ mEnabled,
                        /* selected= */ mSelected,
                        mType,
                        /* protoId= */ 0,
                        /* hasColor= */ mCustomIcon != null);
            } else {
                return new PopupButtonData(
                        (data) -> mOnClicked.run(),
                        mText,
                        mIconId,
                        /* enabled= */ mEnabled,
                        /* selected= */ mSelected,
                        mType,
                        /* protoId= */ 0,
                        /* hasColor= */ false);
            }
        }
    }

    private static void assertEndIconSelected(View button, boolean selected) {
        ImageView endIcon = button.findViewById(R.id.end_icon);
        if (selected) {
            assertEquals(View.VISIBLE, endIcon.getVisibility());
            assertNotNull(endIcon.getDrawable());
        } else {
            assertTrue(endIcon.getVisibility() == View.GONE || endIcon.getDrawable() == null);
        }
    }

    @Test
    public void horizontalAttachments_applyStatefulColors() {
        PropertyModel model = createBottomSheetModel();
        FuseboxViewHolder viewHolder = createBottomSheetViewHolder();
        mBinder.bind(model, viewHolder, FuseboxProperties.COLOR_SCHEME);

        View currentTabButton = viewHolder.popup.mAddCurrentTab;
        View iconBackground = currentTabButton.findViewById(R.id.start_icon_background);
        assertNotNull(iconBackground);

        ColorStateList bgTint = iconBackground.getBackgroundTintList();
        assertNotNull(bgTint);
        assertTrue(bgTint.isStateful());

        TextView textView = currentTabButton.findViewById(R.id.action_text);
        ColorStateList textColors = textView.getTextColors();
        assertNotNull(textColors);
        assertTrue(textColors.isStateful());
    }

    @Test
    public void popupIconTint_plusMenuUsesSecondaryTint() {
        Activity activity = mActivityController.get();
        ColorStateList secondaryTint =
                OmniboxResourceProvider.getSecondaryIconTintList(
                        activity, BrandedColorScheme.APP_DEFAULT);

        ImageView tabIcon = mPopup.mTabButton.findViewById(R.id.start_icon);
        assertEquals(secondaryTint, tabIcon.getImageTintList());

        addModelButton();
        ImageView dynamicIcon = getDynamicButton(0).findViewById(R.id.start_icon);
        assertEquals(secondaryTint, dynamicIcon.getImageTintList());
    }

    @Test
    public void popupIconTint_bottomSheetUsesPrimaryTint() {
        PropertyModel model = createBottomSheetModel();
        FuseboxViewHolder viewHolder = createBottomSheetViewHolder();
        mBinder.bind(model, viewHolder, FuseboxProperties.COLOR_SCHEME);

        ColorStateList primaryTint =
                OmniboxResourceProvider.getPrimaryIconTintList(
                        mActivityController.get(), BrandedColorScheme.APP_DEFAULT);
        ImageView tabIcon = viewHolder.popup.mTabButton.findViewById(R.id.start_icon);
        assertEquals(primaryTint, tabIcon.getImageTintList());

        addModelButton(model, viewHolder);
        ImageView dynamicIcon = getDynamicButton(viewHolder.popup, 0).findViewById(R.id.start_icon);
        assertEquals(primaryTint, dynamicIcon.getImageTintList());
    }

    @Test
    public void popupIconSize_plusMenuUses20dp() {
        Resources res = mActivityController.get().getResources();
        int expectedSize = res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size);

        ImageView tabIcon = mPopup.mTabButton.findViewById(R.id.start_icon);
        ViewGroup.LayoutParams layoutParams = tabIcon.getLayoutParams();
        assertEquals(expectedSize, layoutParams.width);
        assertEquals(expectedSize, layoutParams.height);

        addModelButton();
        ImageView dynamicIcon = getDynamicButton(0).findViewById(R.id.start_icon);
        ViewGroup.LayoutParams dynamicLayoutParams = dynamicIcon.getLayoutParams();
        assertEquals(expectedSize, dynamicLayoutParams.width);
        assertEquals(expectedSize, dynamicLayoutParams.height);
    }

    @Test
    public void popupIconSize_bottomSheetPreserves24dp() {
        PropertyModel model = createBottomSheetModel();
        FuseboxViewHolder viewHolder = createBottomSheetViewHolder();
        mBinder.bind(model, viewHolder, FuseboxProperties.COLOR_SCHEME);
        Resources res = mActivityController.get().getResources();
        int expectedBottomSheetIconSize =
                res.getDimensionPixelSize(R.dimen.fusebox_bottom_sheet_attachment_icon_size);

        ImageView tabIcon = viewHolder.popup.mTabButton.findViewById(R.id.start_icon);
        ViewGroup.LayoutParams layoutParams = tabIcon.getLayoutParams();
        assertEquals(expectedBottomSheetIconSize, layoutParams.width);
        assertEquals(expectedBottomSheetIconSize, layoutParams.height);

        addModelButton(model, viewHolder);
        ImageView dynamicIcon = getDynamicButton(viewHolder.popup, 0).findViewById(R.id.start_icon);
        ViewGroup.LayoutParams dynamicLayoutParams = dynamicIcon.getLayoutParams();
        assertEquals(expectedBottomSheetIconSize, dynamicLayoutParams.width);
        assertEquals(expectedBottomSheetIconSize, dynamicLayoutParams.height);
    }
}
