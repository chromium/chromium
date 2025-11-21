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
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Function;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FuseboxViewHolder mViewHolder;
    @Mock private FuseboxPopup mPopup;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    @Mock private Clipboard mClipboard;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private WebContents mWebContents;
    @Mock private Function<Tab, @Nullable Bitmap> mTabFaviconFactory;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;
    private FuseboxAttachmentModelList mAttachments;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ObservableSupplierImpl<Boolean> mOnCompactModeChangedSupplier =
            new ObservableSupplierImpl<>(false);
    private boolean mCompactModeEnabled;
    private final Bitmap mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

    @Before
    public void setUp() {
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);
        mAutocompleteRequestTypeSupplier =
                new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        ConstraintLayout viewGroup = new ConstraintLayout(activity);
        activity.setContentView(viewGroup);
        LayoutInflater.from(activity).inflate(R.layout.fusebox_layout, viewGroup, true);

        mContext = RuntimeEnvironment.application;
        mResources = mContext.getResources();
        mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

        mViewHolder = new FuseboxViewHolder(viewGroup, mPopup);
        mAttachments = new FuseboxAttachmentModelList();
        mAttachments.setComposeBoxQueryControllerBridge(mComposeBoxQueryControllerBridge);
        mMediator =
                spy(
                        new NavigationAttachmentsMediator(
                                mContext,
                                mWindowAndroid,
                                mModel,
                                mViewHolder,
                                mAttachments,
                                mAutocompleteRequestTypeSupplier,
                                mTabModelSelectorSupplier,
                                mComposeBoxQueryControllerBridge,
                                mOnCompactModeChangedSupplier));
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
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        new FuseboxAttachmentModelList(),
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mOnCompactModeChangedSupplier);
    }

    private void addAttachment(String title) {
        addAttachment(title, "token-" + title);
    }

    private void addAttachment(String title, String token) {
        Tab mockTab = mock(Tab.class);
        when(mockTab.getTitle()).thenReturn(title);
        when(mockTab.getId()).thenReturn(0);
        when(mockTab.getWebContents())
                .thenReturn(null); // This will trigger addTabContextFromCache path
        when(mComposeBoxQueryControllerBridge.addTabContext(mockTab)).thenReturn(token);
        when(mComposeBoxQueryControllerBridge.addTabContextFromCache(0)).thenReturn(token);
        mAttachments.add(FuseboxAttachment.forTab(mockTab, mResources));
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
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn("token").when(mComposeBoxQueryControllerBridge).addTabContext(mTab1);
        mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED).run();
        verify(mComposeBoxQueryControllerBridge).addTabContext(mTab1);
        assertEquals(
                mBitmap,
                ((BitmapDrawable) ((FuseboxAttachment) mAttachments.get(0)).thumbnail).getBitmap());

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
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
    public void activateSearchMode_clearsAttachmentsAndAbandonsSession() {
        addAttachment("title");

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
    public void activateImageGeneration_disablesCurrentTabInput() {
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        ShadowLooper.idleMainLooper();

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));

        mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
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
                        new FuseboxAttachmentModelList(),
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mOnCompactModeChangedSupplier);

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
    public void onToggleAttachmentsPopup_createImagesEligible_showsCreateImageButton() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_createImagesNotEligible_hidesCreateImageButton() {
        doReturn(false).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));
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
    public void getAttachmentTokens_returnsEmptyListWhenEmpty() {
        List<String> tokens = mMediator.getAttachmentTokens();
        assertNotNull(tokens);
        assertTrue(tokens.isEmpty());
    }

    @Test
    public void getAttachmentTokens_returnsTokensAfterAddingAttachment() {
        addAttachment("test");

        var tokens = mMediator.getAttachmentTokens();
        assertNotNull(tokens);
        assertEquals(1, tokens.size());
        assertEquals("token-test", tokens.get(0));
    }

    @Test
    public void getAttachmentTokens_returnsEmptyListAfterRemovingAllAttachments() {
        addAttachment("test");

        // Verify attachment was added
        assertNotNull(mMediator.getAttachmentTokens());
        assertEquals(1, mMediator.getAttachmentTokens().size());

        // Remove all attachments
        mAttachments.clear();

        List<String> tokens = mMediator.getAttachmentTokens();
        assertNotNull(tokens);
        assertTrue(tokens.isEmpty());
    }

    @Test
    public void getAttachmentTokens_returnsMultipleTokensInOrder() {
        addAttachment("tab1");
        addAttachment("tab2");

        var tokens = mMediator.getAttachmentTokens();
        assertNotNull(tokens);
        assertEquals(2, tokens.size());
        assertEquals("token-tab1", tokens.get(0));
        assertEquals("token-tab2", tokens.get(1));
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
        doReturn("token-tab1")
                .when(mComposeBoxQueryControllerBridge)
                .addTabContextFromCache(anyLong());
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();

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
    }

    @Test
    public void testCompactMode() {
        OmniboxFeatures.sCompactFusebox.setForTesting(true);
        recreateMediator();
        Callback<Boolean> compactModeCallback = (val) -> mCompactModeEnabled = val;
        mOnCompactModeChangedSupplier.addObserver(compactModeCallback);

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
}
