// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.AttachmentDetailsFetcher.AttachmentDetails;
import org.chromium.chrome.browser.omnibox.fusebox.NavigationAttachmentsRecyclerViewAdapter.NavigationAttachmentItemType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock NavigationAttachmentsViewHolder mViewHolder;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private @Mock Clipboard mClipboard;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock Bitmap mBitmap;
    private @Mock TabModel mTabModel;
    private @Mock Tab mTab1;
    private @Mock Tab mTab2;
    private @Mock Tab mTab3;
    private @Mock WebContents mWebContents;

    private Activity mActivity;
    private Context mContext;
    private ViewGroup mViewGroup;
    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ModelList mTabAttachmentsModelList = new ModelList();
    private final List<Tab> mTabs = new ArrayList<>();

    @Before
    public void setUp() {
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);
        mAutocompleteRequestTypeSupplier = new ObservableSupplierImpl<>();
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mViewGroup = new ConstraintLayout(mActivity);
        mActivity.setContentView(mViewGroup);
        LayoutInflater.from(mActivity).inflate(R.layout.fusebox_layout, mViewGroup, true);

        mContext = RuntimeEnvironment.application;
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);

        mViewHolder = new NavigationAttachmentsViewHolder(mViewGroup, mPopup);
        mMediator =
                Mockito.spy(
                        new NavigationAttachmentsMediator(
                                mContext,
                                mWindowAndroid,
                                mModel,
                                mViewHolder,
                                new ModelList(),
                                mAutocompleteRequestTypeSupplier,
                                mTabModelSelectorSupplier,
                                mTabAttachmentsModelList,
                                mComposeBoxQueryControllerBridge));
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();
        Clipboard.setInstanceForTesting(mClipboard);
        OmniboxResourceProvider.setTabFaviconFactory((any) -> mBitmap);
    }

    @Test
    public void initialState_toolbarIsHidden() {
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_toolbarVisibleWhenFocused() {
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_viewsHiddenWhenNotFocused() {
        // Show it first
        mMediator.setToolbarVisible(true);
        mMediator.setAutocompleteRequestTypeChangeable(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertTrue(
                mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE));

        // Then hide it
        mMediator.setToolbarVisible(false);
        mMediator.setAutocompleteRequestTypeChangeable(false);
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertFalse(
                mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE));
    }

    @Test
    public void onAddButtonClicked_togglePopup() {
        Runnable runnable = mModel.get(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        doReturn(false).when(mPopup).isShowing();
        runnable.run();
        verify(mPopup).show();

        // Hide popup.
        doReturn(true).when(mPopup).isShowing();
        runnable.run();
        verify(mPopup).dismiss();
    }

    @Test
    public void popupAddsTabs() {
        assertFalse(mModel.get(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE));
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();

        doReturn("Title2").when(mTab2).getTitle();
        doReturn(new GURL("http://www.amazon.com")).when(mTab2).getUrl();
        doReturn(true).when(mTab2).isInitialized();
        doReturn(123L).when(mTab2).getTimestampMillis();

        doReturn("Title3").when(mTab3).getTitle();
        doReturn(new GURL("chrome://flags")).when(mTab3).getUrl();
        doReturn(true).when(mTab3).isInitialized();
        doReturn(true).when(mTab3).isFrozen();
        doReturn(89L).when(mTab3).getTimestampMillis();
        mTabs.add(mTab1);
        mTabs.add(mTab2);
        mTabs.add(mTab3);
        doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();
        doReturn(false).when(mPopup).isShowing();
        mMediator.onToggleAttachmentsPopup();

        assertTrue(mModel.get(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE));
        assertEquals(2, mTabAttachmentsModelList.size());
        assertEquals(
                TabAttachmentPopupChoicesRecyclerViewAdapter.TAB_ATTACHMENT_ITEM_TYPE,
                mTabAttachmentsModelList.get(0).type);
        assertEquals(
                "Title2",
                mTabAttachmentsModelList
                        .get(0)
                        .model
                        .get(TabAttachmentPopupChoiceProperties.TITLE));
        assertEquals(
                TabAttachmentPopupChoicesRecyclerViewAdapter.TAB_ATTACHMENT_ITEM_TYPE,
                mTabAttachmentsModelList.get(1).type);
        assertEquals(
                "Title1",
                mTabAttachmentsModelList
                        .get(1)
                        .model
                        .get(TabAttachmentPopupChoiceProperties.TITLE));

        doReturn(false).when(mTab3).isFrozen();
        doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();
        mMediator.onToggleAttachmentsPopup();
        assertEquals(2, mTabAttachmentsModelList.size());

        doReturn(new GURL("https://www.yahoo.com")).when(mTab3).getUrl();
        doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();
        mMediator.onToggleAttachmentsPopup();
        assertEquals(3, mTabAttachmentsModelList.size());

        doReturn(mWebContents).when(mTab3).getWebContents();
        doReturn("token").when(mComposeBoxQueryControllerBridge).addTabContext(mTab3);
        mTabAttachmentsModelList
                .get(2)
                .model
                .get(TabAttachmentPopupChoiceProperties.ON_CLICK_LISTENER)
                .onClick(null);
        verify(mComposeBoxQueryControllerBridge).addTabContext(mTab3);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
    }

    @Test
    public void onCameraClicked_permissionGranted_launchesCamera() {
        doReturn(true).when(mWindowAndroid).hasPermission(any());
        doNothing().when(mMediator).launchCamera();

        mMediator.onCameraClicked();

        verify(mMediator).launchCamera();
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void onCameraClicked_permissionDenied_requestsPermission() {
        doReturn(false).when(mWindowAndroid).hasPermission(any());
        doNothing().when(mMediator).launchCamera();

        mMediator.onCameraClicked();

        verify(mMediator, never()).launchCamera();
        verify(mWindowAndroid).requestPermissions(any(), any());
    }

    @Test
    public void addAttachment_addAttachment_success() {
        // Success is captured with a valid unique token.
        doReturn("123").when(mComposeBoxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        AttachmentDetails attachmentDetails =
                new AttachmentDetails(
                        NavigationAttachmentItemType.ATTACHMENT_ITEM,
                        null,
                        "title",
                        "image",
                        byteArray);
        mMediator.uploadAndAddAttachment(attachmentDetails);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        verify(mComposeBoxQueryControllerBridge).addFile("title", "image", byteArray);
    }

    @Test
    public void addAttachment_addAttachment_failure() {
        // Failure: no token.
        doReturn(null).when(mComposeBoxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        AttachmentDetails attachmentDetails =
                new AttachmentDetails(
                        NavigationAttachmentItemType.ATTACHMENT_ITEM,
                        null,
                        "title",
                        "image",
                        byteArray);
        mMediator.uploadAndAddAttachment(attachmentDetails);
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
    }

    @Test
    public void activateSearchMode_clearsAttachmentsAndAbandonsSession() {
        ModelList modelList = new ModelList();
        mAutocompleteRequestTypeSupplier =
                new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
        mMediator =
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        modelList,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mTabAttachmentsModelList,
                        mComposeBoxQueryControllerBridge);
        modelList.add(new MVCListAdapter.ListItem(0, new PropertyModel()));
        assertEquals(1, modelList.size());

        mMediator.activateAiMode();
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE));

        mMediator.activateSearchMode();
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        assertEquals(0, modelList.size());
        verify(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(
                AutocompleteRequestType.SEARCH,
                (int) mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void activateAiMode_startsSession() {
        mMediator.activateAiMode();
        verify(mComposeBoxQueryControllerBridge).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void setToolbarVisible_noBridge_doesNothing() {
        // Create a mediator, but don't initialize the bridge.
        NavigationAttachmentsMediator mediator =
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        new ModelList(),
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier,
                        mTabAttachmentsModelList,
                        mComposeBoxQueryControllerBridge);

        // The bridge is not initialized, so no native calls should be made.
        mediator.setToolbarVisible(true);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();

        mediator.setToolbarVisible(false);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionAbandoned();
    }

    @Test
    public void setToolbarVisible_stateNotChanged_doesNothing() {
        // Initial state is false. Calling with false should do nothing.
        mMediator.setToolbarVisible(false);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionAbandoned();

        // Transition to true. Should NOT start a session.
        mMediator.setAutocompleteRequestTypeChangeable(true);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionAbandoned();

        // Manually start a session to test the hiding part.
        mMediator.activateAiMode();
        verify(mComposeBoxQueryControllerBridge).notifySessionStarted();
        Mockito.clearInvocations(mComposeBoxQueryControllerBridge);

        // Calling with true again. Should do nothing.
        mMediator.setAutocompleteRequestTypeChangeable(true);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionAbandoned();

        // Transition to false. Should abandon the session.
        mMediator.setAutocompleteRequestTypeChangeable(false);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        verify(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        Mockito.clearInvocations(mComposeBoxQueryControllerBridge);

        // Calling with false again. Should do nothing.
        mMediator.setAutocompleteRequestTypeChangeable(false);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionAbandoned();
    }

    @Test
    public void onToggleAttachmentsPopup_clipboardHasImage_showsClipboardButton() {
        doReturn(true).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_clipboardDoesNotHaveImage_hidesClipboardButton() {
        doReturn(false).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE));
    }

    @Test
    public void autocompleteRequestTypeClicked_activatesSearchMode() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, (int) mAutocompleteRequestTypeSupplier.get());
    }
}
