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
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.TestActivity;
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

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnchoredPopupWindow mPopupWindow;
    @Mock private DynamicRectProvider mDynamicRectProvider;

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

        mPopup =
                new FuseboxPopup(
                        activity,
                        mPopupWindow,
                        popupView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ false);
        mViewHolder = new FuseboxViewHolder(parent, mPopup);

        // Initialize workable defaults.
        mModel.set(FuseboxProperties.ADD_BUTTON_VISIBLE, true);
        mModel.set(FuseboxProperties.FUSEBOX_STATE, FuseboxState.EXPANDED);
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        mModel.set(FuseboxProperties.SHOW_REQUEST_TYPE_BUTTON, false);
        mModel.set(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);

        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    private View getDynamicButton(int index) {
        ViewGroup group = mPopup.mViewGroup;
        int headerIndex = group.indexOfChild(mPopup.mModelsHeader);
        return group.getChildAt(headerIndex + 1 + index);
    }

    private View getDynamicToolButton(int index) {
        ViewGroup group = mPopup.mViewGroup;
        int headerIndex = group.indexOfChild(mPopup.mToolsHeader);
        return group.getChildAt(headerIndex + 1 + index);
    }

    private void configureFusebox(@Variant int testCase, @AutocompleteRequestType int requestType) {
        // Reflect the active state of the fusebox toolbar.
        mModel.set(
                FuseboxProperties.FUSEBOX_STATE,
                testCase == Variant.COMPACT ? FuseboxState.COMPACT : FuseboxState.EXPANDED);
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, requestType);
        mModel.set(FuseboxProperties.SHOW_REQUEST_TYPE_BUTTON, false);
    }

    @Test
    public void addButtonVisible_setsVisibility() {
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        mModel.set(FuseboxProperties.ADD_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.addButton.getVisibility());

        mModel.set(FuseboxProperties.ADD_BUTTON_VISIBLE, false);
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
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED, runnable);

        mPopup.mCameraButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void galleryButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED, runnable);

        mPopup.mGalleryButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void fileButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED, runnable);

        mPopup.mFileButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void tabPickerButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED, runnable);

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
        assertEquals(View.GONE, mViewHolder.requestType.getVisibility());
    }

    @Test
    public void reanchorViewsForCompactFusebox_compactModeSearch() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(R.id.url_bar, lp.topToTop);
        assertEquals(ConstraintSet.UNSET, lp.topToBottom);
        assertEquals(ConstraintSet.UNSET, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_notCompactMode() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.url_bar, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
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
    public void testClipboardButtonEnabled() {
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED, true);
        assertTrue(mViewHolder.popup.mClipboardButton.isEnabled());
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED, false);
        assertFalse(mViewHolder.popup.mClipboardButton.isEnabled());
    }

    @Test
    public void requestTypeDrawable() {
        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.IMAGE_GENERATION);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[1]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
        assertNull(mViewHolder.requestType.getCompoundDrawablesRelative()[3]);

        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.DEEP_SEARCH);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.CANVAS);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[0]);
        assertNotNull(mViewHolder.requestType.getCompoundDrawablesRelative()[2]);
    }

    @Test
    public void modelButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withOnClicked(runnable).build()));

        getDynamicButton(0).performClick();
        verify(runnable).run();
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
                List.of(new PopupButtonDataBuilder().withEnabled(true).build()));
        assertTrue(getDynamicButton(0).isEnabled());

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(new PopupButtonDataBuilder().withEnabled(false).build()));
        assertFalse(getDynamicButton(0).isEnabled());
    }

    @Test
    public void sendButtonA11y_setsContentDescription() {
        var res = mActivityController.get().getResources();

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.AI_MODE);
        assertEquals(
                res.getString(R.string.acc_send_button_send_to_ai),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(
                res.getString(R.string.acc_send_button_create_image),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.DEEP_SEARCH);
        assertEquals(
                res.getString(R.string.ntp_compose_deep_search),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.CANVAS);
        assertEquals(
                res.getString(R.string.ntp_compose_canvas),
                mViewHolder.navigateButton.getContentDescription());

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, AutocompleteRequestType.SEARCH);
        assertEquals(
                res.getString(R.string.acc_send_button_search_or_navigate),
                mViewHolder.navigateButton.getContentDescription());
    }

    @Test
    public void modelSelectionDrawables() {
        PopupButtonData selectedData = new PopupButtonDataBuilder().withSelected(true).build();
        PopupButtonData notSelectedData = new PopupButtonDataBuilder().withSelected(false).build();
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(notSelectedData, notSelectedData));
        assertEndIconSelected(getDynamicButton(0), false);
        assertEndIconSelected(getDynamicButton(1), false);

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(selectedData, notSelectedData));
        assertEndIconSelected(getDynamicButton(0), true);
        assertEndIconSelected(getDynamicButton(1), false);

        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                List.of(notSelectedData, selectedData));
        assertEndIconSelected(getDynamicButton(0), false);
        assertEndIconSelected(getDynamicButton(1), true);
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
        assertEquals(6, mPopup.mAttachmentButtons.size());
        assertEquals(2, mPopup.mDynamicThemedButtons.size());

        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of(data1));
        assertEquals(1, mPopup.mViewGroup.getChildCount() - (headerIndex + 1));
        assertEquals(6, mPopup.mAttachmentButtons.size());
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
    public void toolSelectionDrawables() {
        PopupButtonData selectedData =
                new PopupButtonDataBuilder()
                        .withSelected(true)
                        .withType(PopupButtonType.TOOL)
                        .build();
        PopupButtonData notSelectedData =
                new PopupButtonDataBuilder()
                        .withSelected(false)
                        .withType(PopupButtonType.TOOL)
                        .build();
        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(notSelectedData, notSelectedData));
        assertEndIconSelected(getDynamicToolButton(0), false);
        assertEndIconSelected(getDynamicToolButton(1), false);

        mModel.set(
                FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST,
                List.of(selectedData, notSelectedData));
        assertEndIconSelected(getDynamicToolButton(0), true);
        assertEndIconSelected(getDynamicToolButton(1), false);
    }

    private static class PopupButtonDataBuilder {
        private Runnable mOnClicked = CallbackUtils.emptyRunnable();
        private String mText = "test";
        private int mIconId;
        private boolean mEnabled = true;
        private boolean mSelected;
        private @PopupButtonType int mType = PopupButtonType.MODEL;

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

        PopupButtonData build() {
            return new PopupButtonData(
                    (data) -> mOnClicked.run(),
                    mText,
                    mIconId,
                    mEnabled,
                    mSelected,
                    mType,
                    /* protoId= */ 0,
                    /* hasColor= */ false);
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
}
