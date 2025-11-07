// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Unit tests for {@link NavigationAttachmentsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsViewBinderUnitTest {
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

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AnchoredPopupWindow mPopupWindow;
    private final PropertyModel mModel =
            new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);

    private Activity mActivity;
    private ConstraintLayout mParent;
    private NavigationAttachmentsViewHolder mViewHolder;
    private NavigationAttachmentsPopup mPopup;
    private ViewGroup mPopupView;

    @Before
    public void setUp() {
        // Replace .create().resume() with .setup() once we have a content view.
        mActivity = Robolectric.buildActivity(TestActivity.class).create().resume().get();

        // Initialize location bar layout
        mParent = new ConstraintLayout(mActivity);
        LayoutInflater.from(mActivity).inflate(R.layout.location_bar, mParent);

        mPopupView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.fusebox_context_popup, null);
        doReturn(mPopupView).when(mPopupWindow).getContentView();

        mPopup = new NavigationAttachmentsPopup(mActivity, mPopupWindow, mPopupView);
        mViewHolder = new NavigationAttachmentsViewHolder(mParent, mPopup);

        // Initialize workable defaults.
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, true);
        mModel.set(
                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.SEARCH);
        mModel.set(NavigationAttachmentsProperties.SHOW_DEDICATED_MODE_BUTTON, false);

        PropertyModelChangeProcessor.create(
                mModel, mViewHolder, NavigationAttachmentsViewBinder::bind);
    }

    private void configureFusebox(@Variant int testCase, @AutocompleteRequestType int requestType) {
        OmniboxFeatures.sShowDedicatedModeButton.setForTesting(
                testCase == Variant.DEDICATED_BUTTON
                        || testCase == Variant.DEDICATED_BUTTON_WITH_HINT);
        OmniboxFeatures.sShowTryAiModeHintInDedicatedModeButton.setForTesting(
                testCase == Variant.DEDICATED_BUTTON_WITH_HINT);

        // Reflect the active state of the fusebox toolbar.
        mModel.set(NavigationAttachmentsProperties.COMPACT_UI, testCase == Variant.COMPACT);
        mModel.set(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE, requestType);
        mModel.set(
                NavigationAttachmentsProperties.SHOW_DEDICATED_MODE_BUTTON,
                OmniboxFeatures.sShowDedicatedModeButton.getValue());
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void toolbarVisible_setsVisibility() {
        mModel.set(
                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.AI_MODE);
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.attachmentsToolbar.getVisibility());

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, false);
        assertEquals(View.GONE, mViewHolder.attachmentsToolbar.getVisibility());
    }

    @Test
    public void attachmentsVisible_setsVisibilityAndTogglesSwitch() {
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, true);
        assertEquals(View.VISIBLE, mViewHolder.attachmentsView.getVisibility());

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, false);
        assertEquals(View.GONE, mViewHolder.attachmentsView.getVisibility());
    }

    @Test
    public void adapter_isSet() {
        SimpleRecyclerViewAdapter adapter = mock(SimpleRecyclerViewAdapter.class);
        mModel.set(NavigationAttachmentsProperties.ADAPTER, adapter);
        assertEquals(adapter, mViewHolder.attachmentsView.getAdapter());
    }

    @Test
    public void addButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        mViewHolder.addButton.performClick();

        verify(runnable).run();
    }

    @Test
    public void cameraButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        mPopup.mCameraButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void galleryButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        mPopup.mGalleryButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void fileButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_FILE_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        mPopup.mFileButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void tabPickerButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_TAB_PICKER_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        mPopup.mTabButton.performClick();
        verify(runnable).run();
    }

    @Test
    public void autocompleteRequestTypeClicked_setsListener() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED, runnable);
        mViewHolder.requestType.performClick();
        verify(runnable).run();
    }

    @Test
    public void updateModeSelectorVisibility_noParams() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        NavigationAttachmentsViewBinder.updateModeSelectorVisibility(mModel, mViewHolder);

        // No button.
        assertEquals(View.GONE, mViewHolder.requestType.getVisibility());
    }

    @Test
    public void updateModeSelectorVisibility_dedicatedButton() {
        configureFusebox(Variant.DEDICATED_BUTTON, AutocompleteRequestType.SEARCH);
        NavigationAttachmentsViewBinder.updateModeSelectorVisibility(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void updateModeSelectorVisibility_dedicatedButtonWithHint_searchMode() {
        configureFusebox(Variant.DEDICATED_BUTTON_WITH_HINT, AutocompleteRequestType.SEARCH);
        NavigationAttachmentsViewBinder.updateModeSelectorVisibility(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("Try AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void updateModeSelectorVisibility_dedicatedButtonWithHint_aiMode() {
        configureFusebox(Variant.DEDICATED_BUTTON_WITH_HINT, AutocompleteRequestType.AI_MODE);
        NavigationAttachmentsViewBinder.updateModeSelectorVisibility(mModel, mViewHolder);

        assertEquals(View.VISIBLE, mViewHolder.requestType.getVisibility());
        assertEquals("AI Mode", mViewHolder.requestType.getText());
    }

    @Test
    public void reanchorViewsForCompactFusebox_compactModeSearch() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.SEARCH);
        NavigationAttachmentsViewBinder.reanchorViewsForCompactFusebox(mModel, mViewHolder);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(R.id.url_bar, lp.topToTop);
        assertEquals(ConstraintSet.UNSET, lp.topToBottom);
        assertEquals(ConstraintSet.UNSET, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_compactModeNotSearch() {
        configureFusebox(Variant.COMPACT, AutocompleteRequestType.AI_MODE);
        NavigationAttachmentsViewBinder.reanchorViewsForCompactFusebox(mModel, mViewHolder);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.url_bar, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
    }

    @Test
    public void reanchorViewsForCompactFusebox_notCompactMode() {
        configureFusebox(Variant.DEFAULT, AutocompleteRequestType.SEARCH);
        NavigationAttachmentsViewBinder.reanchorViewsForCompactFusebox(mModel, mViewHolder);

        var lp = (ConstraintLayout.LayoutParams) mViewHolder.addButton.getLayoutParams();
        assertEquals(ConstraintSet.UNSET, lp.topToTop);
        assertEquals(R.id.url_bar, lp.topToBottom);
        assertEquals(ConstraintSet.PARENT_ID, lp.bottomToBottom);
    }

    @Test
    public void addCurrentTabButton() {
        mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mPopup.mAddCurrentTab.getVisibility());
        mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mPopup.mAddCurrentTab.getVisibility());

        Drawable drawable = Mockito.spy(new ColorDrawable(Color.RED));
        mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_THUMBNAIL, drawable);
        // Verifying via getCompoundDrawables is hard because it requires manipulating the view to
        // resolve its visibility, layout direction, and drawables. This lets us check indirectly.
        verify(drawable).setCallback(mPopup.mAddCurrentTab);
    }
}
