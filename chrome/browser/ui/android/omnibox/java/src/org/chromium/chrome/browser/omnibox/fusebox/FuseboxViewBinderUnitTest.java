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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

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
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Unit tests for {@link FuseboxViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxViewBinderUnitTest {
    @IntDef({
        Variant.DEFAULT,
        Variant.DEDICATED_BUTTON,
        Variant.DEDICATED_BUTTON_WITH_HINT,
        Variant.COMPACT,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface Variant {
        int DEFAULT = 0;
        int DEDICATED_BUTTON = 1;
        int DEDICATED_BUTTON_WITH_HINT = 2;
        int COMPACT = 3;
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnchoredPopupWindow mPopupWindow;

    private final PropertyModel mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

    private ActivityController<TestActivity> mActivityController;
    private FuseboxViewHolder mViewHolder;
    private FuseboxPopup mPopup;

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
        doReturn(popupView).when(mPopupWindow).getContentView();

        mPopup = new FuseboxPopup(activity, mPopupWindow, popupView);
        mViewHolder = new FuseboxViewHolder(parent, mPopup);

        // Initialize workable defaults.
        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, true);
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, false);
        mModel.set(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);

        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    private void configureFusebox(@Variant int testCase, @AutocompleteRequestType int requestType) {
        OmniboxFeatures.sShowDedicatedModeButton.setForTesting(
                testCase == Variant.DEDICATED_BUTTON
                        || testCase == Variant.DEDICATED_BUTTON_WITH_HINT);
        OmniboxFeatures.sShowTryAiModeHintInDedicatedModeButton.setForTesting(
                testCase == Variant.DEDICATED_BUTTON_WITH_HINT);

        // Reflect the active state of the fusebox toolbar.
        mModel.set(FuseboxProperties.COMPACT_UI, testCase == Variant.COMPACT);
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, requestType);
        mModel.set(
                FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON,
                OmniboxFeatures.sShowDedicatedModeButton.getValue());
    }

    @Test
    public void toolbarVisible_setsVisibility() {
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.addButton.getVisibility());

        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, false);
        assertEquals(View.GONE, mViewHolder.addButton.getVisibility());
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
        SimpleRecyclerViewAdapter adapter = mock(SimpleRecyclerViewAdapter.class);
        mModel.set(FuseboxProperties.ADAPTER, adapter);
        assertEquals(adapter, mViewHolder.attachmentsView.getAdapter());
    }

    @Test
    public void addButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.BUTTON_ADD_CLICKED, runnable);

        mViewHolder.addButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void cameraButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_CAMERA_CLICKED, runnable);

        mPopup.mCameraButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void galleryButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_GALLERY_CLICKED, runnable);

        mPopup.mGalleryButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void fileButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_FILE_CLICKED, runnable);

        mPopup.mFileButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void tabPickerButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_TAB_PICKER_CLICKED, runnable);

        mPopup.mTabButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void autocompleteRequestTypeClicked_setsListener() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED, runnable);
        mViewHolder.requestType.performClick();
        verify(runnable).run();
    }

    @Test
    public void updateButtonsVisibility_AndStyling_noParams() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);

        // No button.
        assertEquals(View.GONE, mViewHolder.requestType.getVisibility());
    }

    @Test
    public void updateButtonsVisibility_AndStyling_dedicatedButton() {
        configureFusebox(Variant.DEDICATED_BUTTON, AutocompleteRequestType.SEARCH);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void updateModeSelectorVisibility_dedicatedButtonWithHint_searchModeAndStyling() {
        configureFusebox(Variant.DEDICATED_BUTTON_WITH_HINT, AutocompleteRequestType.SEARCH);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("Try AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void updateModeSelectorVisibility_dedicatedButtonWithHint_aiModeAndStyling() {
        configureFusebox(Variant.DEDICATED_BUTTON_WITH_HINT, AutocompleteRequestType.AI_MODE);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void reanchorViewsForCompactFusebox_compactModeSearch() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);
        FuseboxViewBinder.reanchorViewsForCompactFusebox(mModel, mViewHolder);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(R.id.url_bar, lp.topToTop);
        assertEquals(ConstraintSet.UNSET, lp.topToBottom);
        assertEquals(ConstraintSet.UNSET, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_notCompactMode() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        FuseboxViewBinder.reanchorViewsForCompactFusebox(mModel, mViewHolder);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.url_bar, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
    }

    @Test
    public void requestTypePopupDrawables() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        assertNull(mPopup.mAiModeButton.getCompoundDrawablesRelative()[2]);
        assertNull(mPopup.mCreateImageButton.getCompoundDrawablesRelative()[2]);

        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.IMAGE_GENERATION);
        assertNotNull(mPopup.mAiModeButton.getCompoundDrawablesRelative()[0]);
        assertNull(mPopup.mAiModeButton.getCompoundDrawablesRelative()[2]);
        assertNotNull(mPopup.mCreateImageButton.getCompoundDrawablesRelative()[0]);
        assertNotNull(mPopup.mCreateImageButton.getCompoundDrawablesRelative()[2]);

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertNotNull(mPopup.mAiModeButton.getCompoundDrawablesRelative()[0]);
        assertNotNull(mPopup.mAiModeButton.getCompoundDrawablesRelative()[2]);
        assertNotNull(mPopup.mCreateImageButton.getCompoundDrawablesRelative()[0]);
        assertNull(mPopup.mCreateImageButton.getCompoundDrawablesRelative()[2]);
    }

    @Test
    public void fileButtonVisibility_setsVisibility() {
        mModel.set(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mFileButton.getVisibility());

        mModel.set(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mFileButton.getVisibility());
    }

    @Test
    public void addCurrentTabButton() {
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mAddCurrentTab.getVisibility());
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mAddCurrentTab.getVisibility());

        assertNull(mPopup.mAddCurrentTab.getCompoundDrawables()[0]);

        Bitmap favicon = UiUtils.createBitmap(/* size= */ 1, Color.RED);
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON, favicon);
        Drawable faviconDrawable = mPopup.mAddCurrentTab.getCompoundDrawablesRelative()[0];
        assertNotNull(faviconDrawable);

        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON, null);
        Drawable fallbackDrawable = mPopup.mAddCurrentTab.getCompoundDrawablesRelative()[0];
        assertNotNull(fallbackDrawable);
        assertNotEquals(fallbackDrawable, faviconDrawable);
    }

    @Test
    public void aiModeButtonVisibility() {
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false);
        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false);
        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, false);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.VISIBLE, mPopup.mAiModeButton.getVisibility());
    }

    @Test
    public void createImageButtonVisibility() {
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false);
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE, false);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false);
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.VISIBLE, mPopup.mCreateImageButton.getVisibility());
    }

    @Test
    public void requestTypeDividerVisibility() {
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mRequestTypeDivider.getVisibility());
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, true);
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE, false);
        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.GONE, mPopup.mRequestTypeDivider.getVisibility());
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, false);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.VISIBLE, mPopup.mRequestTypeDivider.getVisibility());
        assertEquals(View.VISIBLE, mPopup.mAiModeButton.getVisibility());
        assertEquals(View.GONE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, true);
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.VISIBLE, mPopup.mRequestTypeDivider.getVisibility());
        assertEquals(View.GONE, mPopup.mAiModeButton.getVisibility());
        assertEquals(View.VISIBLE, mPopup.mCreateImageButton.getVisibility());

        mModel.set(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON, false);
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE, true);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertEquals(View.VISIBLE, mPopup.mRequestTypeDivider.getVisibility());
        assertEquals(View.VISIBLE, mPopup.mAiModeButton.getVisibility());
        assertEquals(View.VISIBLE, mPopup.mCreateImageButton.getVisibility());
    }

    @Test
    public void testCurrentTabButtonEnabled() {
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED, true);
        assertTrue(mViewHolder.popup.mAddCurrentTab.isEnabled());
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED, false);
        assertFalse(mViewHolder.popup.mAddCurrentTab.isEnabled());
    }

    @Test
    public void requestTypeDrawable() {
        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.IMAGE_GENERATION);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        FuseboxViewBinder.updateButtonsVisibilityAndStyling(mModel, mViewHolder);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);
    }
}
