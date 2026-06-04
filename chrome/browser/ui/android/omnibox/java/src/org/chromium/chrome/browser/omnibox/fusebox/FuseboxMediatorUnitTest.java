// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
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

import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.util.ChromeItemPickerExtras;
import org.chromium.components.browser_ui.util.ChromeItemPickerUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
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
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private Function<Tab, @Nullable Bitmap> mTabFaviconFactory;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;
    @Mock private ScrimManager mScrimManager;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private BackPressManager mBackPressManager;
    @Mock private Runnable mOnFirstPickerInteractionCanceledCallback;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<WindowAndroid.IntentCallback> mIntentCallbackCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private FuseboxMediator mMediator;
    private FuseboxAttachmentModelList mAttachments;
    private final LinkedHashMap<Integer, Tab> mTabMap = new LinkedHashMap<>();
    private SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableMonotonicObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<List<SuggestedTabInfo>> mSuggestedTabsSupplier =
            ObservableSuppliers.createNonNull(List.of());
    private final SettableNonNullObservableSupplier<@PopupState Integer> mPopupStateSupplier =
            ObservableSuppliers.createNonNull(PopupState.HIDDEN);

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
        lenient().doReturn(mKeyboardVisibilityDelegate).when(mWindowAndroid).getKeyboardDelegate();

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();
        mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);
        mModel.set(FuseboxProperties.POPUP_STATE, PopupState.HIDDEN);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);

        mViewHolder = new FuseboxViewHolder(viewGroup, mPopup);
        mAttachments = new FuseboxAttachmentModelList();
        mAttachments.setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);
        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFactory);
        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());
        when(mComposeboxQueryControllerBridge.getInputStateSupplier())
                .thenReturn(mInputStateSupplier);
        when(mComposeboxQueryControllerBridge.getSuggestedTabsSupplier())
                .thenReturn(mSuggestedTabsSupplier);
        when(mComposeboxQueryControllerBridge.isCreateImagesEligible()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        mTabMap.clear();
        doAnswer(i -> new ArrayList<>(mTabMap.values()).get(i.getArgument(0)))
                .when(mTabModel)
                .getTabAt(anyInt());
        doAnswer(i -> mTabMap.size()).when(mTabModel).getCount();
        doAnswer(i -> mTabMap.get(i.getArgument(0))).when(mTabModelSelector).getTabById(anyInt());

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
                        mWindowAndroid,
                        mModel,
                        mViewHolder,
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mPopupStateSupplier,
                        mSnackbarManager,
                        mScrimManager,
                        () -> null,
                        mBackPressManager,
                        mOnFirstPickerInteractionCanceledCallback);
        mMediator.beginInput(createSession());
    }

    private FuseboxSessionState createSession() {
        var session = mock(FuseboxSessionState.class);
        var metrics = new FuseboxMetrics();
        lenient().doReturn(mAutocompleteController).when(session).getAutocompleteController();
        lenient().doReturn(mProfile).when(session).getProfile();
        lenient().doReturn(mInput).when(session).getAutocompleteInput();
        lenient()
                .doReturn(mComposeboxQueryControllerBridge)
                .when(session)
                .getComposeboxQueryControllerBridge();
        lenient().doReturn(mAttachments).when(session).getFuseboxAttachmentModelList();
        lenient().doReturn(metrics).when(session).getMetrics();
        return session;
    }

    private void clickToolButton(int protoId) {
        List<PopupButtonData> toolButtons =
                mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        for (PopupButtonData data : toolButtons) {
            if (data.protoId == protoId) {
                data.onClicked.run();
                return;
            }
        }
        throw new IllegalArgumentException("Tool button not found for protoId: " + protoId);
    }

    private boolean isToolVisible(int protoId) {
        List<PopupButtonData> toolButtons =
                mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        if (toolButtons == null) return false;
        for (PopupButtonData data : toolButtons) {
            if (data.protoId == protoId) {
                return true;
            }
        }
        return false;
    }

    private boolean isToolEnabled(int protoId) {
        List<PopupButtonData> toolButtons =
                mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        if (toolButtons == null) return false;
        for (PopupButtonData data : toolButtons) {
            if (data.protoId == protoId) {
                return data.enabled;
            }
        }
        return false;
    }

    private void addTabAttachment(Tab tab) {
        mMediator.uploadAndAddAttachment(
                FuseboxAttachment.forTab(
                        tab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false));
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
            when(mComposeboxQueryControllerBridge.addTabContext(mockTab, false)).thenReturn(token);
            when(mComposeboxQueryControllerBridge.addTabContextFromCache(0, false))
                    .thenReturn(token);
            attachment =
                    FuseboxAttachment.forTab(
                            mockTab,
                            /* bypassTabCache= */ false,
                            mResources,
                            FuseboxAttachmentButtonType.TAB_PICKER,
                            /* isSuggestedTab= */ false);
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
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL) {
            doReturn(token).when(mComposeboxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment =
                    FuseboxAttachment.forImageNoThumbnail(
                            title,
                            "image/",
                            new byte[0],
                            SystemClock.elapsedRealtime(),
                            FuseboxAttachmentButtonType.CAMERA);
        } else if (attachmentType == FuseboxAttachmentType.ATTACHMENT_PDF) {
            doReturn(token).when(mComposeboxQueryControllerBridge).addFile(eq(title), any(), any());
            attachment =
                    FuseboxAttachment.forPdf(
                            /* thumbnail= */ null,
                            title,
                            "application/pdf",
                            new byte[0],
                            SystemClock.elapsedRealtime(),
                            FuseboxAttachmentButtonType.FILES);
        } else {
            throw new UnsupportedOperationException();
        }

        mMediator.uploadAndAddAttachment(attachment);
        RobolectricUtil.runAllBackgroundAndUi();
        return attachment;
    }

    private Tab mockTab(int id) {
        return mockTab(id, /* webContentsReady= */ true);
    }

    private Tab mockTab(int id, GURL url) {
        Tab t = mockTab(id);
        when(t.getUrl()).thenReturn(url);
        when(t.isInitialized()).thenReturn(true);
        when(t.getTimestampMillis()).thenReturn(id * 100L);

        mTabMap.put(id, t);
        return t;
    }

    private Tab mockTab(int id, boolean webContentsReady) {
        Tab tab = mock(Tab.class);
        String token = "token-" + id;
        when(tab.getId()).thenReturn(id);
        when(tab.getWebContents()).thenReturn(webContentsReady ? mWebContents : null);
        when(tab.getTitle()).thenReturn("Tab " + id);
        when(mTabModelSelector.getTabById(id)).thenReturn(tab);

        when(mComposeboxQueryControllerBridge.addTabContext(tab, false)).thenReturn(token);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(id, false)).thenReturn(token);
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
    public void initialState_isDisabled() {
        mMediator.endInput();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void beginInput_withStandbyNoFocusState_isDisabled() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void testBeginInput_FuseboxPopup_ShowsPopup() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);

        mMediator.endInput();
        mMediator.beginInput(createSession());

        assertNotEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void beginInput_isNotDisabled() {
        assertNotEquals(
                FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void startInAiMode_isExpanded() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_desktopPlatform_conventional_emptyModelList_isCompact() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_desktopPlatform_nonConventional_emptyModelList_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_desktopPlatform_nonEmptyModelList_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        recreateMediator();

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void testClickRequestTypeChip_transitionsToCompactWhenHasAttachments() {
        recreateMediator();

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());

        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_notDesktop_textWrapping_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        mMediator.setIsTextWrapping(true);

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_notDesktop_notSearchRequest_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void updateFuseboxState_standbyNoFocus_isDisabled() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();

        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void testActivationChipClicked_TransitionsStandbyToEnabled() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        mModel.get(FuseboxProperties.ACTIVATION_CHIP_CLICKED).run();

        assertEquals(AutocompleteState.ENABLED, mInput.getAutocompleteState());
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_true() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_false_conventional() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_false_desktopAiMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void endInput_clearsState() {
        assertNotEquals(
                FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());

        mMediator.endInput();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
    }

    @Test
    public void onAddButtonClicked_togglePopup() {
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        assertEquals(PopupState.FLOATING, (int) mModel.get(FuseboxProperties.POPUP_STATE));

        // Hide popup.
        runnable.run();
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void onAddButtonClicked_bottomSheet_hidesKeyboard() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mViewHolder.parentView);
    }

    @Test
    public void onAddButtonClicked_updatesPopupStateSupplier() {
        assertEquals(PopupState.HIDDEN, mPopupStateSupplier.get().intValue());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertEquals(PopupState.FLOATING, mPopupStateSupplier.get().intValue());
    }

    @Test
    public void onHidePopup_bottomSheet_showsKeyboardIfFocused() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        ConstraintLayout spyParent = spy(mViewHolder.parentView);
        doReturn(mViewHolder.addButton).when(spyParent).findFocus();
        mViewHolder = new FuseboxViewHolder(spyParent, mPopup);
        recreateMediator();

        // Show popup first
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();

        // Hide popup
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();

        verify(mKeyboardVisibilityDelegate).showKeyboard(any());
    }

    @Test
    public void onAddButtonClicked_floatingPopup_doesNotHideKeyboard() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(false);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
    }

    @Test
    public void testPopupShowHide_triggersScrim() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mScrimManager).showScrim(any());

        // Hide popup.
        runnable.run();
        verify(mScrimManager).hideScrim(any(), eq(true));
    }

    @Test
    public void testDoubleBeginInput_doesNotRecreateScrim() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        verify(mScrimManager).showScrim(any());
        clearInvocations(mScrimManager);

        // Trigger second beginInput on the same mediator (simulating double tap from ntp fakebox).
        mMediator.beginInput(createSession());

        verify(mScrimManager, never()).hideScrim(any(), anyBoolean());
        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testDoubleBeginInput_doesNotReapplyPopupState() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        org.chromium.base.Callback<Integer> observer = mock(org.chromium.base.Callback.class);
        mPopupStateSupplier.addSyncObserverAndCallIfNonNull(observer);
        verify(observer).onResult(PopupState.FLOATING);
        clearInvocations(observer);

        // Trigger second beginInput on the same mediator.
        mMediator.beginInput(createSession());

        verify(observer, never()).onResult(any());
    }

    @Test
    public void testPopupShowHide_floatingMode_doesNotTriggerScrim() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(false);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    public void testPopupShowHide_desktopPlatform_usesFloatingMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        assertEquals(PopupState.FLOATING, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void testEndInput_DismissesPopup() {
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertEquals(PopupState.FLOATING, (int) mModel.get(FuseboxProperties.POPUP_STATE));

        mMediator.endInput();
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void handleHidePopup_popupShowing_hidesAndReturnsTrue() {
        mModel.set(FuseboxProperties.POPUP_STATE, PopupState.FLOATING);

        assertTrue(mMediator.handleHidePopup());
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void handleHidePopup_popupHidden_returnsFalse() {
        mModel.set(FuseboxProperties.POPUP_STATE, PopupState.HIDDEN);

        assertFalse(mMediator.handleHidePopup());
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void testBackPressHandler() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        recreateMediator();
        verify(mBackPressManager).addHandler(mMediator, BackPressHandler.Type.FUSEBOX_POPUP);

        assertFalse(mMediator.getHandleBackPressChangedSupplier().get());

        // Show popup toggles supplier to true.
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mMediator.getHandleBackPressChangedSupplier().get());

        // Handle back press hides popup and returns SUCCESS.
        assertEquals(BackPressResult.SUCCESS, mMediator.handleBackPress());
        assertFalse(mMediator.getHandleBackPressChangedSupplier().get());
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));

        mMediator.destroy();
        verify(mBackPressManager).removeHandler(mMediator);
    }

    @Test
    public void testBackPressHandler_inactive_returnsFailure() {
        recreateMediator();
        assertFalse(mMediator.getHandleBackPressChangedSupplier().get());
        assertEquals(BackPressResult.FAILURE, mMediator.handleBackPress());
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

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNonNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(false);
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(true);
        doReturn(null).when(mTabFaviconFactory).apply(any());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        doReturn(mBitmap).when(mTabFaviconFactory).apply(any());
        doReturn("token").when(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);
        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();
        verify(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);
        assertEquals(mBitmap, ((BitmapDrawable) mAttachments.get(0).thumbnail).getBitmap());

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
    }

    @Test
    public void onCameraClicked_permissionGranted_launchesCamera() {
        doReturn(true).when(mWindowAndroid).hasPermission(any());

        mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED).run();

        verify(mWindowAndroid).showCancelableIntent(any(Intent.class), any(), any());
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void onCameraClicked_permissionDenied_requestsPermission() {
        doReturn(false).when(mWindowAndroid).hasPermission(any());

        mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED).run();

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
    public void dedicatedButton_clearsAttachmentsAndAbandonsSession() {
        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_TAB);
        assertEquals(
                AutocompleteRequestType.AI_MODE, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
        assertEquals(0, mAttachments.size());
    }

    @Test
    public void dedicatedButton_startsSession() {
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        verify(mComposeboxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.AI_MODE, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
    }

    @Test
    public void activateAiMode_fromToolMenu_recordsMetrics() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected",
                        ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);
        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void onToolCreateImageClicked_startsSession() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected",
                        ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        verify(mComposeboxQueryControllerBridge, never()).notifySessionStarted();
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
        histogramWatcher.assertExpected();
    }

    @Test
    public void clickSelectedTool_transitionsToSearchMode() {
        // Initially in Search mode.
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);
        assertEquals(
                AutocompleteRequestType.AI_MODE, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
    }

    @Test
    public void clickSelectedImageGenTool_transitionsToSearchMode() {
        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
    }

    @Test
    public void onToolCreateImageGeneration_disablesNonImageInput() {
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

        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void addAttachment_takesEffectInSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(
                AutocompleteRequestType.AI_MODE, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void addAttachment_doesNotAlterCurrentCustomMode() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void testUpdateVisualsForState_colorScheme() {
        mMediator.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());

        mMediator.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        assertEquals(
                BrandedColorScheme.INCOGNITO,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());

        mMediator.updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());

        mMediator.updateVisualsForState(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT,
                mModel.get(FuseboxProperties.COLOR_SCHEME).intValue());
    }

    @Test
    public void onPlusButtonClicked_pdfUploadEligible_showsFileButton() {
        doReturn(true).when(mComposeboxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
    }

    @Test
    public void onPlusButtonClicked_pdfUploadNotEligible_hidesFileButton() {
        doReturn(false).when(mComposeboxQueryControllerBridge).isPdfUploadEligible();
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testGalleryIntent_extraAllowMultiple() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        assertTrue(mMediator.wasActionTaken());
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
        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ false));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax_duringCreateImage() {
        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertEquals(
                FuseboxAttachmentModelList.getMaxAttachments(),
                intent.getIntExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, /* defaultValue= */ -1));
    }

    @Test
    public void onImagePickerClicked_setsMimeType() {
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.IMAGE_ANY_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void onFilePickerClicked_allFilesOff_setsCorrectMimeType() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.LENS_SEND_RAW_FILE_MEDIA_TYPES, false);
        mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.PDF_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void onFilePickerClicked_allFilesOn_setsCorrectMimeType() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.LENS_SEND_RAW_FILE_MEDIA_TYPES, true);
        mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(MimeTypeUtils.ALL_FILE_TYPES_MIME_TYPE, mIntentCaptor.getValue().getType());
    }

    @Test
    public void requestTypeButtonClicked_activatesSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);

        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void popupToolCanvasClicked_activatesCanvasMode() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();
        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withToolConfigs(new byte[][] {canvasConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected",
                        ToolMode.TOOL_MODE_CANVAS_VALUE);

        clickToolButton(ToolMode.TOOL_MODE_CANVAS_VALUE);

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.CANVAS, mInput.getRequestType());
        histogramWatcher.assertExpected();
    }

    @Test
    public void popupToolDeepSearchClicked_activatesDeepSearchMode() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();
        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .withToolConfigs(new byte[][] {deepSearchConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected",
                        ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE);

        clickToolButton(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE);

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.DEEP_SEARCH, mInput.getRequestType());
        histogramWatcher.assertExpected();
    }

    @Test
    public void popupModelButtonClicked_setsModelMode() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();
        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Flash")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        models.get(0).onClicked.run();

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
        assertEquals(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE, mInput.getModelMode());
        verify(mComposeboxQueryControllerBridge)
                .setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE);
    }

    @Test
    public void popupModelButtonClicked_recordsMetric() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Flash")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelButtonSelected",
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .build();

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        models.get(0).onClicked.run();
        assertTrue(mMediator.wasActionTaken());

        histogramWatcher.assertExpected();
    }

    @Test
    public void testModelPickerVisibility_hidesIfFewerThanTwoModels() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        InputState state0 = new InputState.Builder().build();
        mInputStateSupplier.set(state0);
        assertEquals(0, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        InputState state1 =
                new InputState.Builder()
                        .withAllowedModels(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withModelConfigs(new byte[][] {config1.toByteArray()})
                        .build();
        mInputStateSupplier.set(state1);
        assertEquals(0, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));

        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        InputState state2 =
                new InputState.Builder()
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state2);
        assertEquals(2, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void testModelPickerVisibility_hidesInBottomSheet() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        recreateMediator();

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);
        assertEquals(2, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void onRequestTypeButtonClicked_fromDeepSearch_activatesSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.DEEP_SEARCH);
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void onRequestTypeButtonClicked_fromCanvas_activatesSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.CANVAS);
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void beginInput_fromNtp_recordsAiModeActivationSource() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mInput.setFocusReason(OmniboxFocusReason.NTP_AI_MODE);

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.NTP_BUTTON)) {
            recreateMediator();
        }
    }

    @Test
    public void onToolCreateImageClicked_fromConventional_recordsAiModeActivationSource() {
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU)) {
            clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        }
    }

    @Test
    public void onToolCreateImageClicked_fromAiMode_doesNotRecordAiModeActivationSource() {
        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);

        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AiModeActivationSource")
                        .build()) {
            clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
        }
    }

    @Test
    public void onToolDeepSearchClicked_fromConventional_recordsAiModeActivationSource() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .withToolConfigs(new byte[][] {deepSearchConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU)) {
            clickToolButton(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE);
        }
    }

    @Test
    public void onToolCanvasClicked_fromConventional_recordsAiModeActivationSource() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withToolConfigs(new byte[][] {canvasConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU)) {
            clickToolButton(ToolMode.TOOL_MODE_CANVAS_VALUE);
        }
    }

    @Test
    public void onModelSelected_fromConventional_recordsAiModeActivationSource() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Flash")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertFalse(models.isEmpty());

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.IMPLICIT)) {
            models.get(0).onClicked.run();
        }
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
    public void testUploadAndAddAttachment_nullAttachment_showsSnackbar() {
        mMediator.uploadAndAddAttachment(/* attachment= */ null);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testAddAttachment_disablesCreateImage() {
        doReturn("token-tab1").when(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);
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
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();
        assertEquals(1, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(0, mAttachments.size());
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_FILE);
        assertEquals(1, mAttachments.size());
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(0, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token-pdf", FuseboxAttachmentType.ATTACHMENT_PDF);
        assertEquals(1, mAttachments.size());
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertEquals(0, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token2", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token3", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(2, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        var tabAttachment =
                addAttachment("title-tab", "token-tab", FuseboxAttachmentType.ATTACHMENT_TAB);
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mAttachments.remove(tabAttachment, /* isFailure= */ false);
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        mAttachments.clear();
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token4", FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL);
        assertEquals(1, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        addAttachment("title", "token5", FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL);
        assertEquals(2, mAttachments.size());
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
    }

    @Test
    public void testCompactMode() {
        recreateMediator();
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());

        mMediator.setIsTextWrapping(true);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE).intValue());
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
        assertTrue(mMediator.wasActionTaken());

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
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

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
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
        addTabAttachment(mockTab(101));
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_FILE);

        mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run();

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        int allowedSelectionCount =
                intent.getIntExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, -1);
        // The image and file attachments should count against the max, the tab should not.
        assertEquals(FuseboxAttachmentModelList.getMaxAttachments() - 2, allowedSelectionCount);
    }

    @Test
    public void onTabPickerClicked_sendsAllowedSelectionCount_imageNoThumbnail() {
        addTabAttachment(mockTab(101));
        addAttachment("title1", "token1", FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL);
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_FILE);

        mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run();

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        int allowedSelectionCount =
                intent.getIntExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, -1);
        // The image and file attachments should count against the max, the tab should not.
        assertEquals(FuseboxAttachmentModelList.getMaxAttachments() - 2, allowedSelectionCount);
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
    public void testOnTabPickerResult_resultCanceledWithError_showsSnackbar() {
        Intent intent = new Intent();
        intent.putExtra(ChromeItemPickerExtras.EXTRA_ITEM_PICKER_ERROR, "error message");

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, intent);

        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testOnTabPickerResult_canceled_unfocuses_whenFakeBoxPlusButtonTap() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback).run();
    }

    @Test
    public void testOnTabPickerResult_canceled_doesNotUnfocus_whenNotFakeBoxPlusButtonTap() {
        mInput.setFocusReason(OmniboxFocusReason.OMNIBOX_TAP);
        recreateMediator();

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback, never()).run();
    }

    @Test
    public void testOnTabPickerResult_canceled_doesNotUnfocus_afterAttachmentAdded() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_TAB);

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback, never()).run();
    }

    @Test
    public void testOnTabPickerResult_canceled_doesNotUnfocus_afterToolButtonClicked() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        clickToolButton(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback, never()).run();
    }

    @Test
    public void testOnTabPickerResult_canceled_doesNotUnfocus_afterModelButtonClicked() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        ModelConfig config1 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        ModelConfig config2 =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Flash")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {config1.toByteArray(), config2.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertFalse(models.isEmpty());
        models.get(0).onClicked.run();

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback, never()).run();
    }

    @Test
    public void testCameraResult_nullBitmap_unfocuses_whenFakeBoxPlusButtonTap() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        doReturn(true).when(mWindowAndroid).hasPermission(any());
        recreateMediator();

        mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED).run();

        verify(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), mIntentCallbackCaptor.capture(), any());
        Intent mockIntent = new Intent();
        mIntentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, mockIntent);

        verify(mOnFirstPickerInteractionCanceledCallback).run();
    }

    @Test
    public void testImagePickerResult_emptyUris_unfocuses_whenFakeBoxPlusButtonTap() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();

        verify(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), mIntentCallbackCaptor.capture(), any());
        Intent mockIntent = new Intent();
        mIntentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, mockIntent);

        verify(mOnFirstPickerInteractionCanceledCallback).run();
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_maxAttachmentsReached() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
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
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));

        // Remove one attachment to free up space.
        mAttachments.remove(mAttachments.get(0), /* isFailure= */ false);
        assertTrue(mAttachments.getRemainingAttachments() > 0);
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void testUpdatePopupButtonEnabledStates_modeChanges() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void testPopupCreateImageButtonVisible() {
        doReturn(true).when(mComposeboxQueryControllerBridge).isCreateImagesEligible();
        recreateMediator();
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        doReturn(false).when(mComposeboxQueryControllerBridge).isCreateImagesEligible();
        recreateMediator();
        assertFalse(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
    }

    @Test
    public void testInputStateObserverSubscription() {
        assertFalse(OmniboxFeatures.sShowModelPicker.getValue());
        assertFalse(mInputStateSupplier.hasObservers());

        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();
        assertTrue(mInputStateSupplier.hasObservers());
        mMediator.endInput();
        assertFalse(mInputStateSupplier.hasObservers());
    }

    @Test
    public void testOnInputStateChange() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);

        ModelConfig configAuto =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .setIcon(Icon.newBuilder().setIconId(IconResourceIds.AUTORENEW).build())
                        .build();
        ModelConfig configPro =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .setIcon(Icon.newBuilder().setIconId(IconResourceIds.TIMER).build())
                        .build();
        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE,
                                ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE,
                                ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withModelConfigs(
                                new byte[][] {configAuto.toByteArray(), configPro.toByteArray()})
                        .withToolConfigs(
                                new byte[][] {
                                    deepSearchConfig.toByteArray(), canvasConfig.toByteArray()
                                })
                        .build();
        mInputStateSupplier.set(state);

        assertTrue(isToolVisible(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE));

        assertTrue(isToolVisible(ToolMode.TOOL_MODE_CANVAS_VALUE));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_CANVAS_VALUE));

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());

        PopupButtonData autoData = models.get(0);
        assertEquals("Auto", autoData.text);
        assertEquals(IconResourceIds.AUTORENEW_VALUE, autoData.iconId);
        assertTrue(autoData.enabled);
        assertTrue(autoData.selected);

        PopupButtonData proData = models.get(1);
        assertEquals("Pro", proData.text);
        assertEquals(IconResourceIds.TIMER_VALUE, proData.iconId);
        assertFalse(proData.enabled);
        assertFalse(proData.selected);

        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void testOnInputStateChange_ActiveOverridesDisabled() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig configPro =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        ModelConfig configAuto =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();

        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {configPro.toByteArray(), configAuto.toByteArray()})
                        .withToolConfigs(new byte[][] {canvasConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS_VALUE));
        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        assertTrue(models.get(0).enabled);
    }

    @Test
    public void testOnInputStateChange_ActiveOverridesAllowed() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig configPro =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        ModelConfig configAuto =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();

        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas")
                        .build();
        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withModelConfigs(
                                new byte[][] {configPro.toByteArray(), configAuto.toByteArray()})
                        .withToolConfigs(
                                new byte[][] {
                                    canvasConfig.toByteArray(), deepSearchConfig.toByteArray()
                                })
                        .build();
        mInputStateSupplier.set(state);

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS_VALUE));
        List<PopupButtonData> modelButtons =
                mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, modelButtons.size());
        assertEquals("Pro", modelButtons.get(0).text);
    }

    @Test
    public void modelSelectionProperties_conditionalOnRequestType() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig configPro =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        ModelConfig configAuto =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();

        InputState inputState =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withModelConfigs(
                                new byte[][] {configPro.toByteArray(), configAuto.toByteArray()})
                        .build();
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mInputStateSupplier.set(inputState);
        List<PopupButtonData> modelButtons =
                mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, modelButtons.size());
        assertFalse(modelButtons.get(0).selected);

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        modelButtons = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertTrue(modelButtons.get(0).selected);

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        modelButtons = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertFalse(modelButtons.get(0).selected);
    }

    @Test
    public void onInputStateChange_updatesEnabledStates() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        InputState state =
                new InputState.Builder()
                        .withDisabledInputTypes(
                                InputType.INPUT_TYPE_BROWSER_TAB_VALUE,
                                InputType.INPUT_TYPE_LENS_FILE_VALUE)
                        .build();

        mInputStateSupplier.set(state);

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
    }

    @Test
    public void onInputStateChange_updatesHeaders() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        SectionConfig toolsConfig = SectionConfig.newBuilder().setHeader("Tools Header").build();
        SectionConfig modelConfig = SectionConfig.newBuilder().setHeader("Models Header").build();

        InputState state =
                new InputState.Builder()
                        .withToolsSectionConfig(toolsConfig.toByteArray())
                        .withModelSectionConfig(modelConfig.toByteArray())
                        .build();

        mInputStateSupplier.set(state);

        assertEquals("Tools Header", mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_TEXT));
        assertEquals("Models Header", mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_TEXT));
    }

    @Test
    public void onInputStateChanged_setsCreateImageVisibilityAndEnablement() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ToolConfig imageGenConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setMenuLabel("Image Gen")
                        .build();
        ToolConfig imageGenUploadConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .setMenuLabel("Image Gen Upload")
                        .build();

        InputState bothHidden = new InputState.Builder().build();
        mInputStateSupplier.set(bothHidden);
        assertFalse(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState imageGenVisibleDisabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withToolConfigs(new byte[][] {imageGenConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(imageGenVisibleDisabled);
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState imageGenUploadVisibleDisabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .withToolConfigs(new byte[][] {imageGenUploadConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(imageGenUploadVisibleDisabled);
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE));

        InputState imageGenEnabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withToolConfigs(new byte[][] {imageGenConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(imageGenEnabled);
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState imageGenUploadEnabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .withToolConfigs(new byte[][] {imageGenUploadConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(imageGenUploadEnabled);
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE));
    }

    @Test
    public void onAutocompleteRequestTypeChanged_resetsActiveModel() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ModelConfig proConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        ModelConfig autoConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .setMenuLabel("Auto")
                        .build();
        InputState state =
                new InputState.Builder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .withModelConfigs(
                                new byte[][] {proConfig.toByteArray(), autoConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(state);

        List<FuseboxProperties.PopupButtonData> models =
                mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        assertEquals("Pro", models.get(0).text);
        assertEquals("Auto", models.get(1).text);
        models.get(1).onClicked.run();

        verify(mComposeboxQueryControllerBridge)
                .setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE);
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
        clearInvocations(mComposeboxQueryControllerBridge);

        // The active model should be reset to the default (Pro).
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        verify(mComposeboxQueryControllerBridge)
                .setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE);
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void testReconcileSuggestedTabs() {
        mMediator.beginInput(createSession());
        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);

        SuggestedTabInfo info =
                new SuggestedTabInfo(1, "Title", new GURL("https://google.com"), 12345L);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getTitle()).thenReturn("Title");
        when(mTabModelSelector.getTabById(1)).thenReturn(tab);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(eq(1L), anyBoolean()))
                .thenReturn("token");

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxAttachmentButtonType.SUGGESTED_TAB)
                        .build();

        mSuggestedTabsSupplier.set(List.of(info));
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(1, mAttachments.size());
        assertEquals(1, mAttachments.get(0).getTabId());
        assertTrue(mAttachments.get(0).isSuggestedTab);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testUpdateClientControlledToolButtonList_setsCorrectIcons() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        recreateMediator();
        RobolectricUtil.runAllBackgroundAndUi();

        List<PopupButtonData> toolButtons =
                mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        assertThat(toolButtons).hasSize(2);
        assertEquals(IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE_VALUE, toolButtons.get(0).iconId);
        assertEquals(IconResourceIds.BANANA_VALUE, toolButtons.get(1).iconId);
    }

    @Test
    public void updateModelForRecentTabs_nonDesktop_remainsHidden() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE));
    }

    @Test
    public void updateModelForRecentTabs_desktopPlatform_populatesRecentTabs() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        recreateMediator();
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);

        // Active tab.
        Tab tab1 = mockTab(1, JUnitTestGURLs.GOOGLE_URL);
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab1);

        // Recent tabs.
        Tab tab2 = mockTab(2, JUnitTestGURLs.NTP_URL);
        Tab tab3 = mockTab(3, JUnitTestGURLs.URL_1);
        Tab tab4 = mockTab(4, JUnitTestGURLs.URL_2);

        mModel.get(FuseboxProperties.BUTTON_ADD_CLICKED).run();

        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE));

        List<PopupButtonData> recentTabs =
                mModel.get(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST);
        assertEquals(3, recentTabs.size());
        assertEquals("Tab 4", recentTabs.get(0).text);
        assertEquals("Tab 3", recentTabs.get(1).text);
        assertEquals("Tab 1", recentTabs.get(2).text);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AttachmentButtonUsed",
                        FuseboxAttachmentButtonType.RECENT_TAB);

        recentTabs.get(0).onClicked.run();
        assertEquals(1, mAttachments.size());
        assertEquals(4, mAttachments.get(0).getTabId());
        histogramWatcher.assertExpected();
    }

    @Test
    public void activationChip() {
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertFalse(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));

        mMediator.beginInput(createSession());
        assertTrue(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));

        mInput.setSiteSearchData(new SiteSearchData("test", "Test"));
        assertFalse(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));

        mInput.setSiteSearchData(null);
        assertTrue(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        assertFalse(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));

        mMediator.endInput();
        assertFalse(mModel.get(FuseboxProperties.ACTIVATION_CHIP_VISIBLE));
    }

    @Test
    public void onAutocompleteRequestTypeChanged_clearsAttachments_nonAim() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(1, mAttachments.size());

        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(1, mAttachments.size());

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertEquals(0, mAttachments.size());
    }

    @Test
    public void onAutocompleteRequestTypeChanged_setsRequestTypeButtonText() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        assertEquals(
                mContext.getString(R.string.ai_mode_entrypoint_label),
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(
                mContext.getString(R.string.omnibox_create_image),
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.DEEP_SEARCH);
        assertEquals(
                mContext.getString(R.string.ntp_compose_deep_search),
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.CANVAS);
        assertEquals(
                mContext.getString(R.string.ntp_compose_canvas),
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertEquals("", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
    }

    @Test
    public void onInputStateChanged_setsRequestTypeButtonText_modelPickerEnabled() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        recreateMediator();

        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas Menu")
                        .setChipLabel("Canvas Chip")
                        .build();
        InputState stateWithTool =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withToolConfigs(new byte[][] {canvasConfig.toByteArray()})
                        .build();

        mInputStateSupplier.set(stateWithTool);
        assertEquals("Canvas Chip", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        ToolConfig secondConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setMenuLabel("Canvas Menu")
                        .setChipLabel("Canvas Chip")
                        .build();
        InputState secondInputState =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withToolConfigs(new byte[][] {secondConfig.toByteArray()})
                        .build();
        mInputStateSupplier.set(secondInputState);
        assertEquals("AI Mode", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
    }
}
