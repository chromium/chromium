// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.os.Bundle;
import android.provider.MediaStore;
import android.view.LayoutInflater;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/** Unit tests for {@link FuseboxMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FuseboxViewHolder mViewHolder;
    @Mock private FuseboxPopup mPopup;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    @Mock private Clipboard mClipboard;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private Function<Tab, @Nullable Bitmap> mTabFaviconFactory;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private SnackbarManager mSnackbarManager;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private FuseboxMediator mMediator;
    private FuseboxAttachmentModelList mAttachments;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ObservableSupplierImpl<@FuseboxState Integer> mFuseboxStateSupplier =
            new ObservableSupplierImpl<>(FuseboxState.DISABLED);
    private boolean mCompactModeEnabled;
    private final Bitmap mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

    @Before
    public void setUp() {
        OmniboxFeatures.sMultiattachmentFusebox.setForTesting(true);
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);
        mAutocompleteRequestTypeSupplier =
                new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        ConstraintLayout viewGroup = new ConstraintLayout(activity);
        activity.setContentView(viewGroup);
        LayoutInflater.from(activity).inflate(R.layout.fusebox_layout, viewGroup, true);

        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);

        mContext = RuntimeEnvironment.application;
        mResources = mContext.getResources();
        mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

        mViewHolder = new FuseboxViewHolder(viewGroup, mPopup);
        mAttachments = new FuseboxAttachmentModelList(mTabModelSelectorSupplier);
        mAttachments.setComposeBoxQueryControllerBridge(mComposeBoxQueryControllerBridge);
        mMediator =
                new FuseboxMediator(
                        mContext,
                        mProfile,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        mAttachments,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mFuseboxStateSupplier,
                        mSnackbarManager);
        Clipboard.setInstanceForTesting(mClipboard);
        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFactory);
        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());

        // Start with no init calls.
        clearInvocations(mComposeBoxQueryControllerBridge);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    /* Useful for testing logic in the mediator's constructor. */
    private void recreateMediator() {
        mMediator =
                new FuseboxMediator(
                        mContext,
                        mProfile,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        new FuseboxAttachmentModelList(mTabModelSelectorSupplier),
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mFuseboxStateSupplier,
                        mSnackbarManager);
    }

    private void addTabAttachment(Tab tab) {
        mMediator.uploadAndAddAttachment(FuseboxAttachment.forTab(tab, mResources));
    }

    private FuseboxAttachment addAttachment(
            String title, String token, @FuseboxAttachmentType int attachmentType) {
        FuseboxAttachment attachment;
        if (attachmentType == FuseboxAttachmentType.ATTACHMENT_TAB) {
            Tab mockTab = mock(Tab.class);
            when(mockTab.getTitle()).thenReturn(title);
            when(mockTab.getId()).thenReturn(0);
            when(mockTab.getWebContents())
                    .thenReturn(null); // This will trigger addTabContextFromCache path
            when(mComposeBoxQueryControllerBridge.addTabContext(mockTab)).thenReturn(token);
            when(mComposeBoxQueryControllerBridge.addTabContextFromCache(0)).thenReturn(token);
            attachment = FuseboxAttachment.forTab(mockTab, mResources);
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_FILE) {
            doReturn(token).when(mComposeBoxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment = FuseboxAttachment.forFile(null, title, "image/", new byte[0]);
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE) {
            doReturn(token).when(mComposeBoxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment =
                    FuseboxAttachment.forCameraImage(
                            /* thumbnail= */ null, title, "image/", new byte[0]);
        } else {
            throw new UnsupportedOperationException();
        }

        mMediator.uploadAndAddAttachment(attachment);
        return attachment;
    }

    private Tab mockTab(int id, boolean webContentsReady) {
        Tab tab = mock(Tab.class);
        String token = "token-" + id;
        when(tab.getId()).thenReturn(id);
        when(tab.getWebContents()).thenReturn(webContentsReady ? mWebContents : null);
        when(tab.getTitle()).thenReturn("Tab " + id);
        when(mTabModelSelector.getTabById(id)).thenReturn(tab);

        when(mComposeBoxQueryControllerBridge.addTabContext(tab)).thenReturn(token);
        when(mComposeBoxQueryControllerBridge.addTabContextFromCache(id)).thenReturn(token);
        return tab;
    }

    private Intent createTabPickerResultIntent(List<Integer> tabIds) {
        Intent data = mock(Intent.class);
        Bundle extras = mock(Bundle.class);
        when(data.getExtras()).thenReturn(extras);
        when(data.getIntegerArrayListExtra(FuseboxMediator.EXTRA_ATTACHMENT_TAB_IDS))
                .thenReturn(new ArrayList<>(tabIds));
        return data;
    }

    private Set<Integer> getCurrentlyAttachedIdsFromModel() {
        Set<Integer> ids = new HashSet<>();
        for (int i = 0; i < mAttachments.size(); i++) {
            MVCListAdapter.ListItem listItem = mAttachments.get(i);
            if (listItem.type != FuseboxAttachmentType.ATTACHMENT_TAB) continue;

            FuseboxAttachment attachment =
                    listItem.model.get(FuseboxAttachmentProperties.ATTACHMENT);
            Tab tab = attachment.tab;
            if (tab != null) {
                ids.add(tab.getId());
            }
        }
        return ids;
    }

    @Test
    public void testDestroy() {
        assertTrue(mAutocompleteRequestTypeSupplier.hasObservers());
        mMediator.destroy();
        assertFalse(mAutocompleteRequestTypeSupplier.hasObservers());
    }

    @Test
    public void initialState_toolbarIsHidden() {
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_toolbarVisibleWhenFocused() {
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_startInAiMode() {
        OmniboxFeatures.sCompactFusebox.setForTesting(true);
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.COMPACT_UI));
    }

    @Test
    public void onUrlFocusChange_viewsHiddenWhenNotFocused() {
        // Show it first
        mMediator.setToolbarVisible(true);
        mMediator.setAutocompleteRequestTypeChangeable(true);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE));

        // Then hide it
        mMediator.setToolbarVisible(false);
        mMediator.setAutocompleteRequestTypeChangeable(false);
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE));
    }

    @Test
    public void onAddButtonClicked_togglePopup() {
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
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
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(false).when(mWebContents).isLoading();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();

        doReturn("Title3").when(mTab2).getTitle();
        doReturn(new GURL("chrome://flags")).when(mTab2).getUrl();
        doReturn(true).when(mTab2).isInitialized();
        doReturn(true).when(mTab2).isFrozen();
        doReturn(89L).when(mTab2).getTimestampMillis();
        doReturn(false).when(mPopup).isShowing();

        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertNonNull(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON));

        doReturn(null).when(mTabFaviconFactory).apply(any());
        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertNull(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON));

        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());
        doReturn("token").when(mComposeBoxQueryControllerBridge).addTabContext(mTab1);
        mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED).run();
        verify(mComposeBoxQueryControllerBridge).addTabContext(mTab1);
        assertEquals(mBitmap, ((BitmapDrawable) mAttachments.get(0).thumbnail).getBitmap());

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
    }

    @Test
    public void onCameraClicked_permissionGranted_launchesCamera() {
        doReturn(true).when(mWindowAndroid).hasPermission(any());

        mMediator.onCameraClicked();

        verify(mWindowAndroid).showCancelableIntent(any(Intent.class), any(), any());
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void onCameraClicked_permissionDenied_requestsPermission() {
        doReturn(false).when(mWindowAndroid).hasPermission(any());

        mMediator.onCameraClicked();

        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
        verify(mWindowAndroid).requestPermissions(any(), any());
    }

    @Test
    public void addAttachment_addAttachment_success() {
        // Success is captured with a valid unique token.
        doReturn("123").when(mComposeBoxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        FuseboxAttachment attachment = FuseboxAttachment.forFile(null, "title", "image", byteArray);
        mMediator.uploadAndAddAttachment(attachment);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        verify(mComposeBoxQueryControllerBridge).addFile("title", "image", byteArray);
    }

    @Test
    public void addAttachment_addAttachment_failure() {
        // Failure: no token.
        doReturn(null).when(mComposeBoxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        FuseboxAttachment attachment = FuseboxAttachment.forFile(null, "title", "image", byteArray);
        mMediator.uploadAndAddAttachment(attachment);
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
    }

    @Test
    public void testAddAttachment_replacesDuringCreateImage() {
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());

        mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run();

        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());
        assertEquals("token2", mAttachments.get(0).getToken());
    }

    @Test
    public void activateSearchMode_clearsAttachmentsAndAbandonsSession() {
        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_TAB);

        mMediator.activateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, true);
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));

        mMediator.activateSearchMode();
        assertEquals(
                AutocompleteRequestType.SEARCH,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
        assertEquals(0, mAttachments.size());
    }

    @Test
    public void activateAiMode_startsSession() {
        mMediator.activateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void activateImageGeneration_startsSession() {
        mMediator.activateImageGeneration();
        verify(mComposeBoxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void activateImageGeneration_disablesNonImageInput() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isPdfUploadEligible();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(false).when(mWebContents).isLoading();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        recreateMediator();
        ShadowLooper.idleMainLooper();

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));

        mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
    }

    @Test
    public void maybeActivateAiMode_takesEffectInSearchMode() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mMediator.maybeActivateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
        verify(mPopup, atLeastOnce()).dismiss();
    }

    @Test
    public void maybeActivateAiMode_doesNotAlterCurrentCustomMode() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        mMediator.maybeActivateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
        verify(mPopup).dismiss();
    }

    @Test
    public void testIsIncognito() {
        mMediator.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());

        mMediator.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        assertEquals(
                BrandedColorScheme.INCOGNITO,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());
    }

    @Test
    public void setToolbarVisible_noBridge_doesNothing() {
        ObservableSupplierImpl<Integer> requestTypeSupplier = new ObservableSupplierImpl<>();
        requestTypeSupplier.set(AutocompleteRequestType.SEARCH);

        // Create a mediator, but don't initialize the bridge.
        FuseboxMediator mediator =
                new FuseboxMediator(
                        mContext,
                        mProfile,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        new FuseboxAttachmentModelList(mTabModelSelectorSupplier),
                        requestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mFuseboxStateSupplier,
                        mSnackbarManager);

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
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);

        // Transition to true. Should NOT start a session.
        mMediator.setAutocompleteRequestTypeChangeable(true);
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);

        // Manually start a session to test the hiding part.
        mMediator.activateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);

        // Calling with true again. Should do nothing.
        mMediator.setAutocompleteRequestTypeChangeable(true);
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);

        // Transition to false. Should abandon the session.
        mMediator.setAutocompleteRequestTypeChangeable(false);
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);

        // Calling with false again. Should do nothing.
        mMediator.setAutocompleteRequestTypeChangeable(false);
        verifyNoMoreInteractions(mComposeBoxQueryControllerBridge);
    }

    @Test
    public void onToggleAttachmentsPopup_clipboardHasImage_showsClipboardButton() {
        doReturn(true).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_clipboardDoesNotHaveImage_hidesClipboardButton() {
        doReturn(false).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_pdfUploadEligible_showsFileButton() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_pdfUploadNotEligible_hidesFileButton() {
        doReturn(false).when(mComposeBoxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testGalleryIntent_extraAllowMultiple() {
        mModel.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ false));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax() {
        mModel.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                FuseboxAttachmentModelList.MAX_ATTACHMENTS,
                intent.getIntExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, /* defaultValue= */ -1));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testGalleryIntent_extraAllowMultiple_duringCreateImage() {
        mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertFalse(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ true));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax_duringCreateImage() {
        mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                1, intent.getIntExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, /* defaultValue= */ -1));
    }

    @Test
    public void onImagePickerClicked_setsMimeType() {
        mModel.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.IMAGE_ANY_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void onFilePickerClicked_setsMimeType() {
        mModel.get(FuseboxProperties.POPUP_FILE_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.PDF_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void autocompleteRequestTypeClicked_activatesSearchMode() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, (int) mAutocompleteRequestTypeSupplier.get());
    }

    @Test
    public void testUploadAndAddAttachment_integrationFlow_noCasting() {
        // Setup: Mock successful file upload
        when(mComposeBoxQueryControllerBridge.addFile(anyString(), anyString(), any(byte[].class)))
                .thenReturn("integration-token");

        // Create attachment without token
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        null,
                        "integration-test.txt",
                        "text/plain",
                        "integration content".getBytes());

        // Action: Use mediator's uploadAndAddAttachment method
        mMediator.uploadAndAddAttachment(attachment);

        // Verification: Should work without any casting
        assertEquals(1, mAttachments.size());
        verify(mComposeBoxQueryControllerBridge)
                .addFile(
                        eq("integration-test.txt"),
                        eq("text/plain"),
                        eq("integration content".getBytes()));
        assertEquals("integration-token", attachment.getToken());

        // Verify AI mode is activated
        assertEquals(AutocompleteRequestType.AI_MODE, (int) mAutocompleteRequestTypeSupplier.get());
    }

    @Test
    public void testAddAttachment_disablesCreateImage() {
        doReturn("token-tab1").when(mComposeBoxQueryControllerBridge).addTabContext(mTab1);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(false).when(mTab1).isFrozen();
        doReturn(100L).when(mTab1).getTimestampMillis();
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(false).when(mWebContents).isLoading();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED).run();
        assertEquals(1, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        mAttachments.get(0).model.get(FuseboxAttachmentProperties.ON_REMOVE).run();
        assertEquals(0, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_FILE);
        assertEquals(1, mAttachments.size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        mAttachments.get(0).model.get(FuseboxAttachmentProperties.ON_REMOVE).run();
        assertEquals(0, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        addAttachment("title", "token2", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        addAttachment("title", "token3", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(2, mAttachments.size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));

        mAttachments.get(0).model.get(FuseboxAttachmentProperties.ON_REMOVE).run();
        assertEquals(1, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));
    }

    @Test
    public void testCompactMode() {
        OmniboxFeatures.sCompactFusebox.setForTesting(true);
        recreateMediator();
        Callback<@FuseboxState Integer> compactModeCallback =
                (val) -> mCompactModeEnabled = val == FuseboxState.COMPACT;
        mFuseboxStateSupplier.addObserver(compactModeCallback);

        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.COMPACT_UI));
        assertTrue(mCompactModeEnabled);

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        assertFalse(mModel.get(FuseboxProperties.COMPACT_UI));
        assertFalse(mCompactModeEnabled);

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        assertTrue(mModel.get(FuseboxProperties.COMPACT_UI));
        assertTrue(mCompactModeEnabled);

        mMediator.setUseCompactUi(false);
        assertFalse(mModel.get(FuseboxProperties.COMPACT_UI));
        assertFalse(mCompactModeEnabled);
    }

    @Test
    public void testUpdateCurrentlyAttachedTabs_Reconciliation() {
        Tab tab1 = mockTab(101, /* webContentsReady= */ true);
        mockTab(102, /* webContentsReady= */ false);
        Tab tab3 = mockTab(103, /* webContentsReady= */ true);
        mockTab(104, /* webContentsReady= */ false);

        addTabAttachment(tab1);
        addTabAttachment(tab3);
        assertEquals(new HashSet<>(Arrays.asList(101, 103)), getCurrentlyAttachedIdsFromModel());

        // Create set of newly selected Ids.
        Set<Integer> newlySelectedIds = new HashSet<>(Arrays.asList(102, 103, 104));
        mMediator.updateCurrentlyAttachedTabs(newlySelectedIds);
        assertEquals(newlySelectedIds, getCurrentlyAttachedIdsFromModel());
    }

    @Test
    public void onTabPickerClicked_launchesTabPickerActivity() {
        mModel.get(FuseboxProperties.POPUP_TAB_PICKER_CLICKED).run();

        verify(mPopup).dismiss();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                FuseboxMediator.CHROME_ITEM_PICKER_ACTIVITY_CLASS,
                intent.getComponent().getClassName());
        assertNotNull(intent.getIntegerArrayListExtra(FuseboxMediator.EXTRA_PRESELECTED_TAB_IDS));
        assertEquals(
                FuseboxAttachmentModelList.MAX_ATTACHMENTS,
                intent.getIntExtra(FuseboxMediator.EXTRA_ALLOWED_SELECTION_COUNT, -1));
    }

    @Test
    public void onTabPickerClicked_sendsPreselectedTabIds() {
        Tab tab1 = mockTab(101, /* webContentsReady= */ true);
        Tab tab2 = mockTab(102, /* webContentsReady= */ false);
        addTabAttachment(tab1);
        addTabAttachment(tab2);

        mModel.get(FuseboxProperties.POPUP_TAB_PICKER_CLICKED).run();

        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        ArrayList<Integer> preselectedIds =
                intent.getIntegerArrayListExtra(FuseboxMediator.EXTRA_PRESELECTED_TAB_IDS);
        assertNotNull(preselectedIds);
        assertEquals(2, preselectedIds.size());
        assertTrue(preselectedIds.contains(tab1.getId()));
        assertTrue(preselectedIds.contains(tab2.getId()));
        assertEquals(
                FuseboxAttachmentModelList.MAX_ATTACHMENTS,
                intent.getIntExtra(FuseboxMediator.EXTRA_ALLOWED_SELECTION_COUNT, -1));
    }

    @Test
    public void onTabPickerClicked_sendsAllowedSelectionCount() {
        addTabAttachment(mockTab(101, /* webContentsReady= */ true));
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_FILE);

        mModel.get(FuseboxProperties.POPUP_TAB_PICKER_CLICKED).run();

        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        int allowedSelectionCount =
                intent.getIntExtra(FuseboxMediator.EXTRA_ALLOWED_SELECTION_COUNT, -1);
        // The image and file attachments should count against the max, the tab should not.
        assertEquals(FuseboxAttachmentModelList.MAX_ATTACHMENTS - 2, allowedSelectionCount);
    }

    @Test
    public void testMaxAttachments() {
        for (int i = 0; i < FuseboxAttachmentModelList.MAX_ATTACHMENTS; i++) {
            addAttachment("title", "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }

        assertEquals(FuseboxAttachmentModelList.MAX_ATTACHMENTS, mAttachments.size());

        mockTab(101, /* webContentsReady= */ true);
        mockTab(102, /* webContentsReady= */ false);
        mockTab(103, /* webContentsReady= */ true);
        mockTab(104, /* webContentsReady= */ false);
        Set<Integer> newlySelectedIds = new HashSet<>();
        newlySelectedIds.add(102);
        newlySelectedIds.add(103);
        newlySelectedIds.add(104);
        mMediator.updateCurrentlyAttachedTabs(newlySelectedIds);
        verify(mSnackbarManager).showSnackbar(any());
        assertEquals(FuseboxAttachmentModelList.MAX_ATTACHMENTS, mAttachments.size());

        mMediator.onFilePickerClicked();
        mMediator.onClipboardClicked();
        mMediator.onCameraClicked();
        mMediator.onImagePickerClicked();
        mMediator.onTabPickerClicked();

        verify(mSnackbarManager, times(6)).showSnackbar(any());
        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    public void testOnTabPickerResult_modelListNotEmpty_activatesAiMode() {
        mockTab(101, /* webContentsReady= */ true);
        mockTab(102, /* webContentsReady= */ false);
        ArrayList<Integer> selectedTabIds = new ArrayList<>(Arrays.asList(101, 102));
        Intent resultIntent = createTabPickerResultIntent(selectedTabIds);

        // Add tabs as attachments
        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);
        assertEquals(new HashSet<>(selectedTabIds), getCurrentlyAttachedIdsFromModel());

        // Verify AutocompleteRequestType is AI Mode.
        assertEquals(AutocompleteRequestType.AI_MODE, (int) mAutocompleteRequestTypeSupplier.get());
    }

    @Test
    public void testOnTabPickerResult_modelListEmpty_doesNotActivateAiMode() {
        Intent resultIntent = createTabPickerResultIntent(new ArrayList<>());

        // Set a non-AI mode starting state
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);

        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);
        assertEquals(new HashSet<>(), getCurrentlyAttachedIdsFromModel());

        // AI Mode is NOT activated and AutocompleteRequestType remains SEARCH.
        assertEquals(AutocompleteRequestType.SEARCH, (int) mAutocompleteRequestTypeSupplier.get());
    }

    @Test
    public void testFailedUpload() {
        mMediator.onAttachmentUploadFailed();
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_imageInImageGeneration() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE));
        verify(mSnackbarManager, never()).showSnackbar(any());

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_nonImageInImageGeneration() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);

        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
        verify(mSnackbarManager).showSnackbar(any());

        clearInvocations(mSnackbarManager);

        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_tabReselection() {
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_listFull_noTabs() {
        while (mAttachments.getRemainingAttachments() > 0) {
            addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_FILE);
        }

        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_listFull_withTabs() {
        while (mAttachments.getRemainingAttachments() > 1) {
            addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_FILE);
        }
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_TAB);

        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_remainingAttachments() {
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_FILE));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_maxAttachmentsReached() {
        for (int i = 0; i < FuseboxAttachmentModelList.MAX_ATTACHMENTS; i++) {
            addAttachment("title" + i, "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }
        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_FILE));
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_maxAttachmentsReached() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        assertTrue(mModel.get(FuseboxProperties.POPUP_CAMERA_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_GALLERY_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));

        // Add maximum attachments
        for (int i = 0; i < FuseboxAttachmentModelList.MAX_ATTACHMENTS; i++) {
            addAttachment("file" + i, "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }
        assertEquals(0, mAttachments.getRemainingAttachments());

        assertFalse(mModel.get(FuseboxProperties.POPUP_CAMERA_BUTTON_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_GALLERY_BUTTON_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));

        // Remove one attachment to free up space
        mAttachments.get(0).model.get(FuseboxAttachmentProperties.ON_REMOVE).run();
        assertTrue(mAttachments.getRemainingAttachments() > 0);

        assertTrue(mModel.get(FuseboxProperties.POPUP_CAMERA_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_GALLERY_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_modeChanges() {
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);

        assertTrue(mModel.get(FuseboxProperties.POPUP_CAMERA_BUTTON_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_GALLERY_BUTTON_ENABLED));

        assertFalse(mModel.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
    }
}
