// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.navattach.AttachmentDetailsFetcher.AttachmentDetails;
import org.chromium.chrome.browser.omnibox.navattach.NavigationAttachmentsRecyclerViewAdapter.NavigationAttachmentItemType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ViewGroup mViewGroup;
    private @Mock NavigationAttachmentsViewHolder mViewHolder;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock Profile mProfile;
    private @Mock ComposeBoxQueryControllerBridge.Natives mNativeMock;
    private @Mock Clipboard mClipboard;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock Bitmap mBitmap;
    private @Mock TabModel mTabModel;
    private @Mock Tab mTab1;
    private @Mock Tab mTab2;
    private @Mock Tab mTab3;

    private Context mContext;
    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private final ModelList mTabAttachmentsModelList = new ModelList();
    private final List<Tab> mTabs = new ArrayList<>();

    @Before
    public void setUp() {
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);

        mProfileSupplier = new ObservableSupplierImpl<>(mProfile);
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
                                mProfileSupplier,
                                new ObservableSupplierImpl<>(),
                                mTabModelSelectorSupplier,
                                mTabAttachmentsModelList));
        ComposeBoxQueryControllerBridgeJni.setInstanceForTesting(mNativeMock);
        doReturn(123L).when(mNativeMock).init(mProfile);
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
        mMediator.initializeBridge(mProfile);
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_viewsHiddenWhenNotFocused() {
        mMediator.initializeBridge(mProfile);
        // Show it first
        mMediator.setToolbarVisible(true);
        mMediator.setNavigationTypeVisible(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertTrue(mModel.get(NavigationAttachmentsProperties.NAVIGATION_TYPE_VISIBLE));

        // Then hide it
        mMediator.setToolbarVisible(false);
        mMediator.setNavigationTypeVisible(false);
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertFalse(mModel.get(NavigationAttachmentsProperties.NAVIGATION_TYPE_VISIBLE));
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
    public void addAttachment_addAttachment() {
        mMediator.initializeBridge(mProfile);
        byte[] byteArray = new byte[] {1, 2, 3};
        AttachmentDetails attachmentDetails =
                new AttachmentDetails(
                        NavigationAttachmentItemType.ATTACHMENT_ITEM,
                        null,
                        "title",
                        "image",
                        byteArray);
        mMediator.addAttachment(attachmentDetails);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(byteArray.length);
        byteBuffer.put(byteArray);
        verify(mNativeMock).addFile(123L, "title", "image", byteBuffer);
    }

    @Test
    public void onUseAiModeChanged_off_clearsAttachmentsAndAbandonsSession() {
        ModelList modelList = new ModelList();
        mMediator =
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        modelList,
                        mProfileSupplier,
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier,
                        mTabAttachmentsModelList);
        mMediator.initializeBridge(mProfile);
        modelList.add(new MVCListAdapter.ListItem(0, new PropertyModel()));
        assertEquals(1, modelList.size());

        mMediator.onUseAiModeChanged(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));

        mMediator.onUseAiModeChanged(false);
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        assertEquals(0, modelList.size());
        verify(mNativeMock).notifySessionAbandoned(123L);
    }

    @Test
    public void onUseAiModeChanged_on_startsSession() {
        mMediator.initializeBridge(mProfile);
        mMediator.onUseAiModeChanged(true);
        verify(mNativeMock).notifySessionStarted(123L);
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
                        mProfileSupplier,
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier,
                        mTabAttachmentsModelList);

        // The bridge is not initialized, so no native calls should be made.
        mediator.setToolbarVisible(true);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());

        mediator.setToolbarVisible(false);
        verify(mNativeMock, never()).notifySessionAbandoned(anyLong());
    }

    @Test
    public void setToolbarVisible_stateNotChanged_doesNothing() {
        mMediator.initializeBridge(mProfile);
        // Initial state is false. Calling with false should do nothing.
        mMediator.setToolbarVisible(false);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());
        verify(mNativeMock, never()).notifySessionAbandoned(anyLong());

        // Transition to true. Should NOT start a session.
        mMediator.setNavigationTypeVisible(true);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());
        verify(mNativeMock, never()).notifySessionAbandoned(anyLong());

        // Manually start a session to test the hiding part.
        mMediator.onUseAiModeChanged(true);
        verify(mNativeMock).notifySessionStarted(123L);
        Mockito.clearInvocations(mNativeMock);

        // Calling with true again. Should do nothing.
        mMediator.setNavigationTypeVisible(true);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());
        verify(mNativeMock, never()).notifySessionAbandoned(anyLong());

        // Transition to false. Should abandon the session.
        mMediator.setNavigationTypeVisible(false);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());
        verify(mNativeMock).notifySessionAbandoned(123L);
        Mockito.clearInvocations(mNativeMock);

        // Calling with false again. Should do nothing.
        mMediator.setNavigationTypeVisible(false);
        verify(mNativeMock, never()).notifySessionStarted(anyLong());
        verify(mNativeMock, never()).notifySessionAbandoned(anyLong());
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
}
