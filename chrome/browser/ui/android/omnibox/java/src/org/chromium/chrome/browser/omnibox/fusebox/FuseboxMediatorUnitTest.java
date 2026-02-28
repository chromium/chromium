// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static com.google.common.truth.Truth.assertThat;

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
import static org.mockito.Mockito.verify;
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
import android.os.SystemClock;
import android.provider.MediaStore;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.core.app.ApplicationProvider;

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
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.util.ChromeItemPickerExtras;
import org.chromium.components.browser_ui.util.ChromeItemPickerUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
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
    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    @Mock private Clipboard mClipboard;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private Function<Tab, @Nullable Bitmap> mTabFaviconFactory;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private FuseboxMediator mMediator;
    private FuseboxAttachmentModelList mAttachments;
    private SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private boolean mCompactModeEnabled;
    private final Bitmap mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    private final AutocompleteInput mInput = new AutocompleteInput();

    @Before
    public void setUp() {
        OmniboxFeatures.sMultiattachmentFusebox.setForTesting(true);
        mTabModelSelectorSupplier = ObservableSuppliers.createNonNull(mTabModelSelector);
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        ConstraintLayout viewGroup = new ConstraintLayout(activity);
        activity.setContentView(viewGroup);
        LayoutInflater.from(activity).inflate(R.layout.fusebox_layout, viewGroup, true);

        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        TrackerFactory.setTrackerForTests(mTracker);

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();
        mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

        mViewHolder = new FuseboxViewHolder(viewGroup, mPopup);
        mAttachments = new FuseboxAttachmentModelList();
        mAttachments.setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);
        Clipboard.setInstanceForTesting(mClipboard);
        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFactory);
        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());

        mInput.setPageClassification(
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);
        recreateMediator();

        // Start with no init calls.
        clearInvocations(mComposeboxQueryControllerBridge);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    /* Useful for testing logic in the mediator's constructor. */
    private void recreateMediator() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        mMediator =
                new FuseboxMediator(
                        mContext,
                        mProfile,
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mSnackbarManager);
        mMediator.beginInput(createSession());
    }

    private FuseboxSessionState createSession() {
        return new FuseboxSessionState(mInput, mComposeboxQueryControllerBridge, mAttachments);
    }

    private void addTabAttachment(Tab tab) {
        mMediator.uploadAndAddAttachment(
                FuseboxAttachment.forTab(
                        tab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER));
        RobolectricUtil.runAllBackgroundAndUi();
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
            when(mComposeboxQueryControllerBridge.addTabContext(mockTab)).thenReturn(token);
            when(mComposeboxQueryControllerBridge.addTabContextFromCache(0)).thenReturn(token);
            attachment =
                    FuseboxAttachment.forTab(
                            mockTab,
                            /* bypassTabCache= */ false,
                            mResources,
                            FuseboxAttachmentButtonType.TAB_PICKER);
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_FILE) {
            doReturn(token).when(mComposeboxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment =
                    FuseboxAttachment.forFile(
                            null,
                            title,
                            "image/",
                            new byte[0],
                            SystemClock.elapsedRealtime(),
                            FuseboxAttachmentButtonType.FILES);
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE) {
            doReturn(token).when(mComposeboxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment =
                    FuseboxAttachment.forImage(
                            /* thumbnail= */ null,
                            title,
                            "image/",
                            new byte[0],
                            SystemClock.elapsedRealtime(),
                            FuseboxAttachmentButtonType.CAMERA);
        } else {
            throw new UnsupportedOperationException();
        }

        mMediator.uploadAndAddAttachment(attachment);
        RobolectricUtil.runAllBackgroundAndUi();
        return attachment;
    }

    private Tab mockTab(int id, boolean webContentsReady) {
        Tab tab = mock(Tab.class);
        String token = "token-" + id;
        when(tab.getId()).thenReturn(id);
        when(tab.getWebContents()).thenReturn(webContentsReady ? mWebContents : null);
        when(tab.getTitle()).thenReturn("Tab " + id);
        when(mTabModelSelector.getTabById(id)).thenReturn(tab);

        when(mComposeboxQueryControllerBridge.addTabContext(tab)).thenReturn(token);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(id)).thenReturn(token);
        return tab;
    }

    private Intent createTabPickerResultIntent(List<Integer> tabIds) {
        Intent data = mock(Intent.class);
        Bundle extras = mock(Bundle.class);
        when(data.getExtras()).thenReturn(extras);
        when(data.getIntegerArrayListExtra(ChromeItemPickerExtras.EXTRA_ATTACHMENT_TAB_IDS))
                .thenReturn(new ArrayList<>(tabIds));
        return data;
    }

    @Test
    public void testDestroy() {
        // Use a temp for mock to avoid DirectInvocationOnMock lint check. This test cases uses a
        // mock for mAttachments but the rest of this test file does not.
        FuseboxAttachmentModelList mockAttachments = mock(FuseboxAttachmentModelList.class);
        when(mockAttachments.iterator()).thenReturn(Collections.emptyIterator());
        mAttachments = mockAttachments;
        recreateMediator();

        assertTrue(mInput.getRequestTypeSupplier().hasObservers());
        verify(mAttachments).addObserver(any());

        mMediator.destroy();

        assertFalse(mInput.getRequestTypeSupplier().hasObservers());
        verify(mAttachments).removeObserver(any());
    }

    @Test
    public void initialState_toolbarIsHidden() {
        mMediator.endInput();
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
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.COMPACT_UI));
    }

    @Test
    public void onUrlFocusChange_viewsHiddenWhenNotFocused() {
        // Show it first.
        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));

        // Then hide it.
        mMediator.setToolbarVisible(false);
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE));
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
    public void testEndInput_DismissesPopup() {
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        verify(mPopup).show();

        mMediator.endInput();
        verify(mPopup).dismiss();
    }

    @Test
    public void popupAddsTabs() {
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
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
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNonNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(false);
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(true);
        doReturn(null).when(mTabFaviconFactory).apply(any());
        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());
        doReturn("token").when(mComposeboxQueryControllerBridge).addTabContext(mTab1);
        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();
        verify(mComposeboxQueryControllerBridge).addTabContext(mTab1);
        assertEquals(mBitmap, ((BitmapDrawable) mAttachments.get(0).thumbnail).getBitmap());

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
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
        doReturn("123").when(mComposeboxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        /* thumbnail= */ null,
                        "title",
                        "image",
                        byteArray,
                        SystemClock.elapsedRealtime(),
                        FuseboxAttachmentButtonType.FILES);
        mMediator.uploadAndAddAttachment(attachment);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        verify(mComposeboxQueryControllerBridge).addFile("title", "image", byteArray);
    }

    @Test
    public void addAttachment_addAttachment_failure() {
        // Failure: no token.
        doReturn(null).when(mComposeboxQueryControllerBridge).addFile(any(), any(), any());
        byte[] byteArray = new byte[] {1, 2, 3};
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        /* thumbnail= */ null,
                        "title",
                        "image",
                        byteArray,
                        SystemClock.elapsedRealtime(),
                        FuseboxAttachmentButtonType.FILES);
        mMediator.uploadAndAddAttachment(attachment);
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
    }

    @Test
    public void testAddAttachment_replacesDuringCreateImage() {
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());

        mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED).run();

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
        verify(mComposeboxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void activateImageGeneration_startsSession() {
        mMediator.activateImageGeneration();
        verify(mComposeboxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
    }

    @Test
    public void activateImageGeneration_disablesNonImageInput() {
        doReturn(true).when(mComposeboxQueryControllerBridge).isPdfUploadEligible();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(100L).when(mTab1).getTimestampMillis();
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(false).when(mWebContents).isLoading();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();

        recreateMediator();
        RobolectricUtil.runAllBackgroundAndUi();

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));

        mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void maybeActivateAiMode_takesEffectInSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.maybeActivateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (int) mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE));
        verify(mPopup, atLeastOnce()).dismiss();
    }

    @Test
    public void maybeActivateAiMode_doesNotAlterCurrentCustomMode() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
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
    public void onToggleAttachmentsPopup_clipboardHasImage_showsClipboardButton() {
        doReturn(true).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_clipboardDoesNotHaveImage_hidesClipboardButton() {
        doReturn(false).when(mClipboard).hasImage();
        mMediator.onToggleAttachmentsPopup();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_pdfUploadEligible_showsFileButton() {
        doReturn(true).when(mComposeboxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
    }

    @Test
    public void onToggleAttachmentsPopup_pdfUploadNotEligible_hidesFileButton() {
        doReturn(false).when(mComposeboxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testGalleryIntent_extraAllowMultiple() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ false));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                FuseboxAttachmentModelList.getMaxAttachments(),
                intent.getIntExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, /* defaultValue= */ -1));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testGalleryIntent_extraAllowMultiple_duringCreateImage() {
        mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertFalse(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ true));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax_duringCreateImage() {
        mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                1, intent.getIntExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, /* defaultValue= */ -1));
    }

    @Test
    public void onImagePickerClicked_setsMimeType() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.IMAGE_ANY_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void onFilePickerClicked_setsMimeType() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.PDF_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void autocompleteRequestTypeClicked_activatesSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void testUploadAndAddAttachment_integrationFlow_noCasting() {
        // Setup: Mock successful file upload
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any(byte[].class)))
                .thenReturn("integration-token");

        // Create attachment without token
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        null,
                        "integration-test.txt",
                        "text/plain",
                        "integration content".getBytes(),
                        SystemClock.elapsedRealtime(),
                        FuseboxAttachmentButtonType.FILES);

        // Action: Use mediator's uploadAndAddAttachment method
        mMediator.uploadAndAddAttachment(attachment);

        // Verification: Should work without any casting
        assertEquals(1, mAttachments.size());
        verify(mComposeboxQueryControllerBridge)
                .addFile(
                        eq("integration-test.txt"),
                        eq("text/plain"),
                        eq("integration content".getBytes()));
        assertEquals("integration-token", attachment.getToken());

        // Verify AI mode is activated
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
    }

    @Test
    public void testAddAttachment_disablesCreateImage() {
        doReturn("token-tab1").when(mComposeboxQueryControllerBridge).addTabContext(mTab1);
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
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();
        assertEquals(1, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(0, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_FILE);
        assertEquals(1, mAttachments.size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(0, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        addAttachment("title", "token2", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        addAttachment("title", "token3", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(2, mAttachments.size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(1, mAttachments.size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED));
    }

    @Test
    public void testCompactMode() {
        OmniboxFeatures.sCompactFusebox.setForTesting(true);
        recreateMediator();
        Callback<@FuseboxState Integer> compactModeCallback =
                (val) -> mCompactModeEnabled = val == FuseboxState.COMPACT;
        mFuseboxStateSupplier.addSyncObserverAndCallIfNonNull(compactModeCallback);

        mMediator.setToolbarVisible(true);
        assertTrue(mModel.get(FuseboxProperties.COMPACT_UI));
        assertTrue(mCompactModeEnabled);

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        assertFalse(mModel.get(FuseboxProperties.COMPACT_UI));
        assertFalse(mCompactModeEnabled);

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
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
        assertThat(mAttachments.getAttachedTabIds()).containsExactly(101, 103);

        // Create set of newly selected Ids.
        Set<Integer> newlySelectedIds = new HashSet<>(Arrays.asList(102, 103, 104));
        mMediator.updateCurrentlyAttachedTabs(newlySelectedIds);
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(mAttachments.getAttachedTabIds()).containsExactlyElementsIn(newlySelectedIds);
    }

    @Test
    public void onTabPickerClicked_launchesTabPickerActivity() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run();

        verify(mPopup).dismiss();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                ChromeItemPickerUtils.ACTIVITY_CLASS_NAME, intent.getComponent().getClassName());
        assertNotNull(
                intent.getIntegerArrayListExtra(ChromeItemPickerExtras.EXTRA_PRESELECTED_TAB_IDS));
        assertEquals(
                FuseboxAttachmentModelList.getMaxAttachments(),
                intent.getIntExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, -1));
    }

    @Test
    public void onTabPickerClicked_sendsPreselectedTabIds() {
        Tab tab1 = mockTab(101, /* webContentsReady= */ true);
        Tab tab2 = mockTab(102, /* webContentsReady= */ false);
        addTabAttachment(tab1);
        addTabAttachment(tab2);

        mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run();

        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        ArrayList<Integer> preselectedIds =
                intent.getIntegerArrayListExtra(ChromeItemPickerExtras.EXTRA_PRESELECTED_TAB_IDS);
        assertNotNull(preselectedIds);
        assertEquals(2, preselectedIds.size());
        assertTrue(preselectedIds.contains(tab1.getId()));
        assertTrue(preselectedIds.contains(tab2.getId()));
        assertEquals(
                FuseboxAttachmentModelList.getMaxAttachments(),
                intent.getIntExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, -1));
    }

    @Test
    public void onTabPickerClicked_sendsAllowedSelectionCount() {
        addTabAttachment(mockTab(101, /* webContentsReady= */ true));
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_FILE);

        mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run();

        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        int allowedSelectionCount =
                intent.getIntExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, -1);
        // The image and file attachments should count against the max, the tab should not.
        assertEquals(FuseboxAttachmentModelList.getMaxAttachments() - 2, allowedSelectionCount);
    }

    @Test
    public void testMaxAttachments() {
        for (int i = 0; i < FuseboxAttachmentModelList.getMaxAttachments(); i++) {
            addAttachment("title", "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }

        assertEquals(FuseboxAttachmentModelList.getMaxAttachments(), mAttachments.size());

        mockTab(101, /* webContentsReady= */ true);
        mockTab(102, /* webContentsReady= */ false);
        mockTab(103, /* webContentsReady= */ true);
        mockTab(104, /* webContentsReady= */ false);
        Set<Integer> newlySelectedIds = new HashSet<>();
        newlySelectedIds.add(102);
        newlySelectedIds.add(103);
        newlySelectedIds.add(104);
        mMediator.updateCurrentlyAttachedTabs(newlySelectedIds);
        assertEquals(FuseboxAttachmentModelList.getMaxAttachments(), mAttachments.size());

        mMediator.onFilePickerClicked();
        mMediator.onClipboardClicked();
        mMediator.onCameraClicked();
        mMediator.onImagePickerClicked();
        mMediator.onTabPickerClicked();

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
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(mAttachments.getAttachedTabIds()).containsExactlyElementsIn(selectedTabIds);

        // Verify AutocompleteRequestType is AI Mode.
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
    }

    @Test
    public void testOnTabPickerResult_modelListEmpty_doesNotActivateAiMode() {
        Intent resultIntent = createTabPickerResultIntent(new ArrayList<>());

        // Set a non-AI mode starting state
        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(mAttachments.getAttachedTabIds()).isEmpty();

        // AI Mode is NOT activated and AutocompleteRequestType remains SEARCH.
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void testFailedUpload() {
        mMediator.onAttachmentUploadFailed();
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_imageInImageGeneration() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE));
        verify(mSnackbarManager, never()).showSnackbar(any());

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertFalse(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testIsMaxAttachmentCountReached_nonImageInImageGeneration() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);

        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));

        clearInvocations(mSnackbarManager);

        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB));
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
        for (int i = 0; i < FuseboxAttachmentModelList.getMaxAttachments(); i++) {
            addAttachment("title" + i, "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }
        assertTrue(mMediator.isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_FILE));
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_maxAttachmentsReached() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));

        // Add maximum attachments.
        for (int i = 0; i < FuseboxAttachmentModelList.getMaxAttachments(); i++) {
            addAttachment("file" + i, "token" + i, FuseboxAttachmentType.ATTACHMENT_FILE);
        }
        assertEquals(0, mAttachments.getRemainingAttachments());
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));

        // Remove one attachment to free up space.
        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertTrue(mAttachments.getRemainingAttachments() > 0);
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_modeChanges() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void testPopupCreateImageButtonVisible() {
        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(true);
        doReturn(true).when(mComposeboxQueryControllerBridge).isCreateImagesEligible();
        doReturn(false).when(mProfile).isIncognitoBranded();
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));

        doReturn(false).when(mComposeboxQueryControllerBridge).isCreateImagesEligible();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));
    }

    @Test
    public void testPopupCreateImageButtonVisible_incognitoBranded() {
        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(false);
        doReturn(true).when(mComposeboxQueryControllerBridge).isCreateImagesEligible();
        doReturn(true).when(mProfile).isIncognitoBranded();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(true);
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));
    }
}
