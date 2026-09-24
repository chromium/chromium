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
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

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
import android.view.KeyEvent;
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
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.SetActiveModelSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.AnchoringMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.BackgroundStyle;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService;
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
import org.chromium.components.contextual_search.InputStateBuilder;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AimModelsProtoIntDef.ModelMode;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.DisplayState;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.IconResourceIdsProtoIntDef;
import org.chromium.components.omnibox.InputSourceProto.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/** Unit tests for {@link FuseboxMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxMediatorUnitTest {
    private static final Bitmap BITMAP = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

    private static final InputTypeConfig GALLERY_INPUT_TYPE_CONFIG =
            InputTypeConfig.newBuilder()
                    .setInputType(InputType.INPUT_TYPE_LENS_IMAGE)
                    .setMenuLabel("Gallery")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.ADD_PHOTO_ALTERNATE))
                    .setInputSource(InputSource.INPUT_SOURCE_GALLERY)
                    .build();
    private static final InputTypeConfig CAMERA_INPUT_TYPE_CONFIG =
            InputTypeConfig.newBuilder()
                    .setInputType(InputType.INPUT_TYPE_LENS_IMAGE)
                    .setMenuLabel("Camera")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.LENS_CAMERA))
                    .setInputSource(InputSource.INPUT_SOURCE_CAMERA)
                    .build();
    private static final InputTypeConfig FILES_INPUT_TYPE_CONFIG =
            InputTypeConfig.newBuilder()
                    .setInputType(InputType.INPUT_TYPE_LENS_FILE)
                    .setMenuLabel("Files")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.ATTACH_FILE))
                    .setInputSource(InputSource.INPUT_SOURCE_FILE_PICKER)
                    .build();
    private static final ToolConfig IMAGE_GEN_TOOL_CONFIG =
            ToolConfig.newBuilder()
                    .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                    .setMenuLabel("Create images")
                    .setChipLabel("Create")
                    .setHintText("Describe your image")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.BANANA))
                    .setMenuTooltip("Create images, illustrations, and art")
                    .build();
    private static final ToolConfig IMAGE_GEN_UPLOAD_TOOL_CONFIG =
            ToolConfig.newBuilder()
                    .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                    .setMenuLabel("Image Gen Upload")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.IMAGE_CREATE))
                    .build();
    private static final ToolConfig CANVAS_TOOL_CONFIG =
            ToolConfig.newBuilder()
                    .setToolValue(ToolMode.TOOL_MODE_CANVAS)
                    .setDisableActiveModelSelection(true)
                    .setMenuLabel("Canvas")
                    .setChipLabel("Canvas")
                    .setHintText("Create anything")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.DRAFT_SPARK))
                    .setMenuTooltip("A workspace to create, edit, and save your progress")
                    .build();
    private static final ToolConfig DEEP_SEARCH_TOOL_CONFIG =
            ToolConfig.newBuilder()
                    .setToolValue(ToolMode.TOOL_MODE_DEEP_SEARCH)
                    .setMenuLabel("Deep Search")
                    .setChipLabel("Deep Search")
                    .setHintText("Research anything")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.TRAVEL_EXPLORE))
                    .setMenuTooltip("Research a topic in depth")
                    .build();
    private static final ModelConfig AUTO_MODEL_CONFIG =
            ModelConfig.newBuilder()
                    .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE)
                    .setMenuLabel("Auto")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.AUTORENEW))
                    .setMenuTooltip("Intelligently routes to the best model for your request")
                    .build();
    private static final ModelConfig PRO_MODEL_CONFIG =
            ModelConfig.newBuilder()
                    .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO)
                    .setMenuLabel("Pro")
                    .setIcon(Icon.newBuilder().setIconId(IconResourceIds.TIMER))
                    .setMenuTooltip("Best for complex logic and math")
                    .build();
    private static final SectionConfig TOOLS_SECTION_CONFIG =
            SectionConfig.newBuilder().setHeader("Tools").build();
    private static final SectionConfig MODELS_SECTION_CONFIG =
            SectionConfig.newBuilder().setHeader("Gemini 3 models").build();
    private static final InputState DEFAULT_INPUT_STATE =
            new InputStateBuilder()
                    .withHintText("Ask anything")
                    .withMaxTotalInputs(10)
                    .withAllowedInputTypes(
                            InputType.INPUT_TYPE_LENS_IMAGE_VALUE,
                            InputType.INPUT_TYPE_LENS_FILE_VALUE,
                            InputType.INPUT_TYPE_BROWSER_TAB_VALUE)
                    .withInputTypeConfigs(
                            GALLERY_INPUT_TYPE_CONFIG,
                            CAMERA_INPUT_TYPE_CONFIG,
                            FILES_INPUT_TYPE_CONFIG)
                    .withAllowedTools(
                            ToolMode.TOOL_MODE_IMAGE_GEN,
                            ToolMode.TOOL_MODE_CANVAS,
                            ToolMode.TOOL_MODE_DEEP_SEARCH)
                    .withToolConfigs(
                            IMAGE_GEN_TOOL_CONFIG,
                            IMAGE_GEN_UPLOAD_TOOL_CONFIG,
                            CANVAS_TOOL_CONFIG,
                            DEEP_SEARCH_TOOL_CONFIG)
                    .withToolsSectionConfig(TOOLS_SECTION_CONFIG)
                    .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE)
                    .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE)
                    .withAllowedModels(
                            ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE,
                            ModelMode.MODEL_MODE_GEMINI_PRO)
                    .withModelConfigs(AUTO_MODEL_CONFIG, PRO_MODEL_CONFIG)
                    .withModelSectionConfig(MODELS_SECTION_CONFIG)
                    .build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private FuseboxViewHolder mViewHolder;
    @Mock private FuseboxPopup mPopup;
    @Mock private Profile mProfile;
    @Mock private FuseboxSessionState mSession;
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
    @Mock private KeyEvent mKeyEvent;
    @Mock private Runnable mOnRemoveRunnable;
    @Mock private FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    @Mock private Tab mTab;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock private DriveFilePickerClient mDriveFilePickerClient;
    @Mock private TabLoadingService mTabLoadingService;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<WindowAndroid.IntentCallback> mIntentCallbackCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private FuseboxMediator mMediator;
    private FuseboxAttachmentModelList mAttachments;
    private OmniboxResourceProvider mResourceProvider;
    private SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    private final LinkedHashMap<Integer, Tab> mTabMap = new LinkedHashMap<>();
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableMonotonicObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<List<SuggestedTabInfo>> mSuggestedTabsSupplier =
            ObservableSuppliers.createNonNull(List.of());
    private final SettableNonNullObservableSupplier<@PopupState Integer> mPopupStateSupplier =
            ObservableSuppliers.createNonNull(PopupState.HIDDEN);
    private final SettableNonNullObservableSupplier<Boolean> mHasAttachmentsSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mUrlTextWrappingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final AutocompleteInput mInput = new AutocompleteInput();

    @Before
    public void setUp() {
        OmniboxFeatures.sMultiattachmentFusebox.setForTesting(/* overrideValue= */ true);
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
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);
        mModel.set(FuseboxProperties.POPUP_STATE, PopupState.HIDDEN);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);

        mViewHolder = new FuseboxViewHolder(viewGroup, mPopup);
        mAttachments = new FuseboxAttachmentModelList();
        mAttachments.setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);
        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFactory);
        lenient().doReturn(BITMAP).when(mTabFaviconFactory).apply(any());
        lenient()
                .when(mComposeboxQueryControllerBridge.getInputStateSupplier())
                .thenReturn(mInputStateSupplier);
        lenient()
                .when(mComposeboxQueryControllerBridge.getSuggestedTabsSupplier())
                .thenReturn(mSuggestedTabsSupplier);
        lenient().when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        mTabMap.clear();
        lenient()
                .doAnswer(i -> new ArrayList<>(mTabMap.values()).get(i.getArgument(0)))
                .when(mTabModel)
                .getTabAt(anyInt());
        lenient().doAnswer(i -> mTabMap.size()).when(mTabModel).getCount();
        lenient()
                .doAnswer(i -> mTabMap.get(i.getArgument(0)))
                .when(mTabModelSelector)
                .getTabById(anyInt());

        mInput.setPageClassification(PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);

        lenient().doReturn(mAutocompleteController).when(mSession).getAutocompleteController();
        lenient().doReturn(mProfile).when(mSession).getProfile();
        lenient().doAnswer(i -> mInput).when(mSession).getAutocompleteInput();
        lenient()
                .doReturn(mComposeboxQueryControllerBridge)
                .when(mSession)
                .getComposeboxQueryControllerBridge();
        lenient().doAnswer(i -> mAttachments).when(mSession).getFuseboxAttachmentModelList();
        lenient().doAnswer(i -> new FuseboxMetrics()).when(mSession).getMetrics();
        lenient()
                .when(mComposeboxQueryControllerBridge.addTabContext(any(), anyBoolean()))
                .thenAnswer(i -> "token-" + ((Tab) i.getArgument(0)).getId());
        lenient()
                .when(
                        mComposeboxQueryControllerBridge.addTabContextFromCache(
                                anyLong(), anyBoolean()))
                .thenAnswer(i -> "token-" + i.getArgument(0));
        DriveFilePickerClient.setInstanceForTesting(mDriveFilePickerClient);
        TabLoadingService.setInstanceForTesting(mTabLoadingService);
        lenient().doReturn(true).when(mDriveFilePickerClient).isAvailable(any(), any());
        lenient()
                .doReturn(Promise.fulfilled(null))
                .when(mDriveFilePickerClient)
                .launchPicker(any(), any(), any());

        mInputStateSupplier.set(DEFAULT_INPUT_STATE);

        recreateMediator();

        // Start with no init calls.
        clearInvocations(mComposeboxQueryControllerBridge);
    }

    @After
    public void tearDown() {
        if (mResourceProvider != null) {
            mResourceProvider.destroy();
        }
        mActivityController.close();
    }

    private static InputStateBuilder createDefaultInputStateBuilder() {
        return new InputStateBuilder(DEFAULT_INPUT_STATE);
    }

    private void setInputState(InputStateBuilder builder) {
        mInputStateSupplier.set(builder.build());
    }

    private void setInputState(InputState inputState) {
        mInputStateSupplier.set(inputState);
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
                        mResourceProvider,
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mPopupStateSupplier,
                        mSnackbarManager,
                        mScrimManager,
                        SupplierUtils.ofNull(),
                        mBackPressManager,
                        mOnFirstPickerInteractionCanceledCallback,
                        mHasAttachmentsSupplier,
                        mUrlTextWrappingSupplier);
        mMediator.beginInput(mSession);
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
        assertTrue(mHasAttachmentsSupplier.get());
        return attachment;
    }

    private Tab mockTab(int id) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getTitle()).thenReturn("Tab " + id);
        mTabMap.put(id, tab);
        return tab;
    }

    private Tab mockTab(int id, boolean webContentsReady) {
        return mockTab(id);
    }

    private Tab mockTab(int id, GURL url) {
        Tab t = mockTab(id);
        when(t.getUrl()).thenReturn(url);
        when(t.isInitialized()).thenReturn(true);
        lenient().when(t.getTimestampMillis()).thenReturn(id * 100L);
        lenient().when(t.getWebContents()).thenReturn(mWebContents);
        return t;
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
        mAttachments = mFuseboxAttachmentModelList;
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
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void testAutocompleteStateChange_updatesFuseboxState() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setAutocompleteState(AutocompleteState.ENABLED);
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void beginInput_withStandbyNoFocusState_isDisabled() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void testBeginInput_FuseboxPopup_ShowsPopup() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);

        mMediator.endInput();
        mMediator.beginInput(mSession);

        assertNotEquals(PopupState.HIDDEN, mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void beginInput_isNotDisabled() {
        assertNotEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void startInAiMode_isExpanded() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_desktopPlatform_conventional_emptyModelList_isCompact() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_desktopPlatform_nonConventional_emptyModelList_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void plusButtonBackground_aiModePopover() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        recreateMediator();

        assertEquals(
                BackgroundStyle.ALWAYS_VISIBLE_WIDE,
                mModel.get(FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE));
    }

    @Test
    public void plusButtonBackground_imageGenPopover() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        recreateMediator();

        assertEquals(
                BackgroundStyle.INTERACT_ONLY_SMALL,
                mModel.get(FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE));
    }

    @Test
    public void plusButtonBackground_aiModeToolbar() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);
        recreateMediator();

        assertEquals(
                BackgroundStyle.INTERACT_ONLY_SMALL,
                mModel.get(FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE));
    }

    @Test
    public void updateFuseboxState_desktopPlatform_nonEmptyModelList_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void testClickRequestTypeChip_transitionsToCompactWhenHasAttachments() {

        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_notDesktop_textWrapping_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        mUrlTextWrappingSupplier.set(true);

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_popover_textWrapping_remainsCompact() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        recreateMediator();

        mUrlTextWrappingSupplier.set(true);

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_urlTextWrappingSupplier_updatesState() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mUrlTextWrappingSupplier.set(true);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mUrlTextWrappingSupplier.set(false);
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_notDesktop_notSearchRequest_isExpanded() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_standby_isCompact() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_standbyNoFocus_isDisabled() {
        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();

        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_phone_draftingNoFocus_isDisabled() {
        mInput.setDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        recreateMediator();

        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_phone_drafting_isCompact() {
        mInput.setDisplayState(DisplayState.DRAFTING);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateFuseboxState_phone_aiMode_drafting_isCompact() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mInput.setDisplayState(DisplayState.DRAFTING);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void updateFuseboxState_tablet_draftingNoFocus_isDisabled() {
        mInput.setDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        recreateMediator();

        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void updateFuseboxState_tablet_drafting_isCompact() {
        mInput.setDisplayState(DisplayState.DRAFTING);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void updateFuseboxState_tablet_aiMode_drafting_isCompact() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mInput.setDisplayState(DisplayState.DRAFTING);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void updateAnchoringMode_suggestionsPopover_anchoringModeIsPopover() {
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        recreateMediator();

        assertEquals(AnchoringMode.POPOVER, mModel.get(FuseboxProperties.ANCHORING_MODE));
    }

    @Test
    public void updateAnchoringMode_toolbarMode_compactOrDisabled_anchoringModeIsSingleLine() {
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);
        mInput.setAutocompleteState(AutocompleteState.STANDBY);
        recreateMediator();

        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));
        assertEquals(
                AnchoringMode.TOOLBAR_SINGLE_LINE, mModel.get(FuseboxProperties.ANCHORING_MODE));

        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        recreateMediator();

        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
        assertEquals(
                AnchoringMode.TOOLBAR_SINGLE_LINE, mModel.get(FuseboxProperties.ANCHORING_MODE));
    }

    @Test
    public void updateAnchoringMode_toolbarMode_expanded_anchoringModeIsMultiLine() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mModel.set(FuseboxProperties.FUSEBOX_LAYOUT_MODE, FuseboxLayoutMode.TOOLBAR);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        mUrlTextWrappingSupplier.set(true);

        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
        assertEquals(
                AnchoringMode.TOOLBAR_MULTI_LINE, mModel.get(FuseboxProperties.ANCHORING_MODE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_true() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_false_conventional() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_false_desktopAiMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void updateFuseboxState_setsRequestTypeButtonVisible_false_standby() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        mInput.setAutocompleteState(AutocompleteState.STANDBY);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE));
    }

    @Test
    public void endInput_clearsState() {
        assertNotEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertTrue(mHasAttachmentsSupplier.get());

        mMediator.endInput();
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
        assertFalse(mHasAttachmentsSupplier.get());
    }

    @Test
    public void onPlusButtonClicked_togglePopup() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        assertEquals(PopupState.FLOATING, mModel.get(FuseboxProperties.POPUP_STATE));

        // Hide popup.
        runnable.run();
        assertEquals(PopupState.HIDDEN, mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void onPlusButtonClicked_bottomSheet_hidesKeyboard() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mViewHolder.parentView);
    }

    @Test
    public void onPlusButtonClicked_updatesPopupStateSupplier() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        assertEquals(PopupState.HIDDEN, mPopupStateSupplier.get().intValue());
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        assertEquals(PopupState.FLOATING, mPopupStateSupplier.get().intValue());
    }

    @Test
    public void onHidePopup_bottomSheet_showsKeyboardIfFocused() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        ConstraintLayout spyParent = spy(mViewHolder.parentView);
        doReturn(mViewHolder.plusButton).when(spyParent).findFocus();
        mViewHolder = new FuseboxViewHolder(spyParent, mPopup);
        recreateMediator();

        // Show popup first
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();

        // Hide popup
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();

        verify(mKeyboardVisibilityDelegate).showKeyboard(any());
    }

    @Test
    public void onPlusButtonClicked_floatingPopup_doesNotHideKeyboard() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
    }

    @Test
    public void testPopupShowHide_triggersScrim() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
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
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        verify(mScrimManager).showScrim(any());
        clearInvocations(mScrimManager);

        // Trigger second beginInput on the same mediator (simulating double tap from ntp fakebox).
        mMediator.beginInput(mSession);

        verify(mScrimManager, never()).hideScrim(any(), anyBoolean());
        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    public void testDoubleBeginInput_doesNotReapplyPopupState() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        @SuppressWarnings("unchecked")
        org.chromium.base.Callback<Integer> observer = mock(org.chromium.base.Callback.class);
        mPopupStateSupplier.addSyncObserverAndCallIfNonNull(observer);
        verify(observer).onResult(PopupState.FLOATING);
        clearInvocations(observer);

        // Trigger second beginInput on the same mediator.
        mMediator.beginInput(mSession);

        verify(observer, never()).onResult(any());
    }

    @Test
    public void testPopupShowHide_floatingMode_doesNotTriggerScrim() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    public void testPopupShowHide_desktopPlatform_usesFloatingMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        recreateMediator();
        Runnable runnable = mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        runnable.run();
        assertEquals(PopupState.FLOATING, (int) mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void moreOptionsVisible_accordionDisabled_isGone() {
        OmniboxFeatures.setUseAccordionForTesting(false);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.POPUP_MORE_OPTIONS_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));
    }

    @Test
    public void moreOptionsVisible_accordionEnabled_isVisible() {
        OmniboxFeatures.setUseAccordionForTesting(true);
        recreateMediator();

        assertTrue(mModel.get(FuseboxProperties.POPUP_MORE_OPTIONS_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));
    }

    @Test
    public void testEndInput_DismissesPopup() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ false);
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
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
        OmniboxFeatures.setShowBottomSheetPopupForTesting(/* value= */ true);
        recreateMediator();
        verify(mBackPressManager).addHandler(mMediator, BackPressHandler.Type.FUSEBOX_POPUP);

        assertFalse(mMediator.getHandleBackPressChangedSupplier().get());

        // Show popup toggles supplier to true.
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
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
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNonNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(/* overrideValue= */ false);
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));

        OmniboxFeatures.sAllowCurrentTab.setForTesting(/* overrideValue= */ true);
        doReturn(null).when(mTabFaviconFactory).apply(any());
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE));
        assertNull(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON));

        doReturn(BITMAP).when(mTabFaviconFactory).apply(any());
        doReturn("token").when(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);
        assertFalse(mMediator.wasPopupItemSelected());
        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();
        assertTrue(mMediator.wasPopupItemSelected());
        verify(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);
        assertEquals(BITMAP, ((BitmapDrawable) mAttachments.get(0).thumbnail).getBitmap());

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
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
        assertTrue(mHasAttachmentsSupplier.get());
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
        assertFalse(mHasAttachmentsSupplier.get());
    }

    @Test
    public void attachmentsVisibility_hiddenWhenInStandbyNoFocus() {
        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_TAB);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));

        mInput.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        assertEquals(FuseboxState.DISABLED, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setAutocompleteState(AutocompleteState.ENABLED);
        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
    }

    @Test
    public void dedicatedButton_clearsAttachmentsAndAbandonsSession() {
        addAttachment("title", "token1", FuseboxAttachmentType.ATTACHMENT_TAB);
        assertEquals(AutocompleteRequestType.AI_MODE, mModel.get(FuseboxProperties.REQUEST_TYPE));

        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, mModel.get(FuseboxProperties.REQUEST_TYPE));
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
        mMediator.onPlusButtonClicked();

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected", ToolMode.TOOL_MODE_UNSPECIFIED);
        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED);
        histogramWatcher.assertExpected();
    }

    @Test
    public void clickSelectedTool_transitionsToSearchMode() {
        mMediator.onPlusButtonClicked();

        // Initially in Search mode.
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED);
        assertEquals(
                AutocompleteRequestType.AI_MODE, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));

        mMediator.onPlusButtonClicked();
        clickToolButton(ToolMode.TOOL_MODE_UNSPECIFIED);
        assertEquals(
                AutocompleteRequestType.SEARCH, (int) mModel.get(FuseboxProperties.REQUEST_TYPE));
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
                mModel.get(FuseboxProperties.REQUEST_TYPE));
        assertEquals(PopupState.HIDDEN, mModel.get(FuseboxProperties.POPUP_STATE));
    }

    @Test
    public void testUpdateVisualsForState_colorScheme() {
        mMediator.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        assertEquals(BrandedColorScheme.APP_DEFAULT, mModel.get(FuseboxProperties.COLOR_SCHEME));

        mMediator.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        assertEquals(BrandedColorScheme.INCOGNITO, mModel.get(FuseboxProperties.COLOR_SCHEME));

        mMediator.updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(BrandedColorScheme.APP_DEFAULT, mModel.get(FuseboxProperties.COLOR_SCHEME));

        mMediator.updateVisualsForState(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(BrandedColorScheme.APP_DEFAULT, mModel.get(FuseboxProperties.COLOR_SCHEME));
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
        assertTrue(mMediator.wasPopupItemSelected());
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
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run();
        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        Intent intent = mIntentCaptor.getValue();
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, /* defaultValue= */ false));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGalleryIntent_extraPickImagesMax_duringCreateImage() {
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
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
    public void onDrivePickerClicked_hidesPopupAndNotifiesMetrics() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AttachmentButtonUsed",
                        FuseboxAttachmentButtonType.DRIVE_FILES);

        mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_CLICKED).run();

        assertTrue(mMediator.wasPopupItemSelected());
        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        watcher.assertExpected();
    }

    @Test
    public void onDrivePickerClicked_pickerSuccess_attachesDriveFile() {
        DriveAttachmentMetadata metadata =
                new DriveAttachmentMetadata(
                        "drive_id",
                        /* resourceKey= */ null,
                        "Test Doc",
                        DriveIconUtils.MIME_TYPE_GOOGLE_DOCS);
        doReturn(Promise.fulfilled(metadata))
                .when(mDriveFilePickerClient)
                .launchPicker(mWindowAndroid, mProfile, null);
        doReturn("token123")
                .when(mComposeboxQueryControllerBridge)
                .addDriveFile("drive_id", null, "Test Doc", DriveIconUtils.MIME_TYPE_GOOGLE_DOCS);

        mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_CLICKED).run();
        ShadowLooper.idleMainLooper();

        assertTrue(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        assertEquals(1, mAttachments.size());
        assertEquals("Test Doc", mAttachments.get(0).title);
    }

    @Test
    public void onDrivePickerClicked_pickerCanceled_handlesPickerCanceled() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();

        mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_CLICKED).run();
        ShadowLooper.idleMainLooper();

        assertFalse(mModel.get(FuseboxProperties.ATTACHMENTS_VISIBLE));
        assertEquals(0, mAttachments.size());
        verify(mOnFirstPickerInteractionCanceledCallback).run();
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
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onPlusButtonClicked();

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected", ToolMode.TOOL_MODE_CANVAS);

        clickToolButton(ToolMode.TOOL_MODE_CANVAS);

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.CANVAS, mInput.getRequestType());
        histogramWatcher.assertExpected();
    }

    @Test
    public void popupToolDeepSearchClicked_activatesDeepSearchMode() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onPlusButtonClicked();

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected", ToolMode.TOOL_MODE_DEEP_SEARCH);

        clickToolButton(ToolMode.TOOL_MODE_DEEP_SEARCH);

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.DEEP_SEARCH, mInput.getRequestType());
        histogramWatcher.assertExpected();
    }

    @Test
    public void popupModelButtonClicked_setsModelMode() {
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        models.get(0).onClicked.run();

        assertEquals(PopupState.HIDDEN, (int) mModel.get(FuseboxProperties.POPUP_STATE));
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
        assertEquals(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE, mInput.getModelMode());
        verify(mComposeboxQueryControllerBridge)
                .setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE);
    }

    @Test
    public void popupModelButtonClicked_recordsMetric() {
        mMediator.onPlusButtonClicked();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelButtonSelected",
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE)
                        .build();

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        models.get(0).onClicked.run();
        assertTrue(mMediator.wasPopupItemSelected());

        histogramWatcher.assertExpected();
    }

    @Test
    public void testModelPickerVisibility_hidesIfFewerThanTwoModels() {
        setInputState(createDefaultInputStateBuilder().withAllowedModels().withModelConfigs());
        mMediator.onPlusButtonClicked();
        assertEquals(0, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedModels(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE)
                        .withModelConfigs(AUTO_MODEL_CONFIG));
        assertEquals(0, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));

        setInputState(DEFAULT_INPUT_STATE);
        assertEquals(2, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void testToolVisibility_hidesIfNoTools_inputStateMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();

        setInputState(createDefaultInputStateBuilder().withAllowedTools().withToolConfigs());
        mMediator.onPlusButtonClicked();
        assertEquals(0, mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE));

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withToolConfigs(IMAGE_GEN_TOOL_CONFIG)
                        .withToolsSectionConfig(
                                SectionConfig.newBuilder().setHeader("Tools").build()));
        assertEquals(1, mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST).size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE));
        assertEquals("Tools", mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_TEXT));
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
    public void onToolCanvasClicked_fromAiMode_doesNotRecordAiModeActivationSource() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onPlusButtonClicked();

        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AiModeActivationSource")
                        .build()) {
            clickToolButton(ToolMode.TOOL_MODE_CANVAS);
        }
    }

    @Test
    public void onToolDeepSearchClicked_fromConventional_recordsAiModeActivationSource() {
        mMediator.onPlusButtonClicked();

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU)) {
            clickToolButton(ToolMode.TOOL_MODE_DEEP_SEARCH);
        }
    }

    @Test
    public void onToolCanvasClicked_fromConventional_recordsAiModeActivationSource() {
        mMediator.onPlusButtonClicked();

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU)) {
            clickToolButton(ToolMode.TOOL_MODE_CANVAS);
        }
    }

    @Test
    public void onModelSelected_fromConventional_recordsAiModeActivationSource() {
        mMediator.onPlusButtonClicked();

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
    public void testCompactMode() {
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        assertEquals(FuseboxState.COMPACT, mModel.get(FuseboxProperties.FUSEBOX_STATE));

        mUrlTextWrappingSupplier.set(true);
        assertEquals(FuseboxState.EXPANDED, mModel.get(FuseboxProperties.FUSEBOX_STATE));
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
        assertTrue(mMediator.wasPopupItemSelected());

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

        assertFalse(mMediator.wasPopupItemSelected());
        // Add tabs as attachments
        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);
        assertTrue(mMediator.wasPopupItemSelected());
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
        mMediator.onPlusButtonClicked();

        clickToolButton(ToolMode.TOOL_MODE_CANVAS);

        mMediator.onTabPickerResult(Activity.RESULT_CANCELED, null);

        verify(mOnFirstPickerInteractionCanceledCallback, never()).run();
    }

    @Test
    public void testOnTabPickerResult_canceled_doesNotUnfocus_afterModelButtonClicked() {
        mInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP);
        recreateMediator();
        mMediator.onPlusButtonClicked();

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
    public void testInputStateObserverSubscription() {
        assertTrue(mInputStateSupplier.hasObservers());
        mMediator.endInput();
        assertFalse(mInputStateSupplier.hasObservers());
    }

    @Test
    public void testOnInputStateChange() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);

        setInputState(
                createDefaultInputStateBuilder()
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_PRO));
        mMediator.onPlusButtonClicked();

        assertTrue(isToolVisible(ToolMode.TOOL_MODE_DEEP_SEARCH));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_DEEP_SEARCH));

        assertTrue(isToolVisible(ToolMode.TOOL_MODE_CANVAS));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));

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

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_CANVAS)
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG, AUTO_MODEL_CONFIG));
        mMediator.onPlusButtonClicked();

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));
        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        assertTrue(models.get(0).enabled);
    }

    @Test
    public void testOnInputStateChange_ActiveOverridesAllowed() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_CANVAS)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG, AUTO_MODEL_CONFIG));
        mMediator.onPlusButtonClicked();

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));
        List<PopupButtonData> modelButtons =
                mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, modelButtons.size());
        assertEquals("Pro", modelButtons.get(0).text);
    }

    @Test
    public void modelSelectionProperties_conditionalOnRequestType() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG, AUTO_MODEL_CONFIG));
        mInput.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onPlusButtonClicked();
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

        setInputState(
                createDefaultInputStateBuilder()
                        .withDisabledInputTypes(
                                InputType.INPUT_TYPE_BROWSER_TAB_VALUE,
                                InputType.INPUT_TYPE_LENS_FILE_VALUE));
        mMediator.onPlusButtonClicked();

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_ENABLED));
    }

    @Test
    public void onInputStateChange_updatesDriveButton() {
        FeatureOverrides.overrideFlag(
                OmniboxFeatureList.COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION, true);
        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedInputTypes(InputType.INPUT_TYPE_DRIVE_VALUE)
                        .withDisabledInputTypes(InputType.INPUT_TYPE_DRIVE_VALUE));
        mMediator.onPlusButtonClicked();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_ENABLED));

        setInputState(DEFAULT_INPUT_STATE);
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_ENABLED));
    }

    @Test
    public void onInputStateChange_driveNotAvailable_hidesDriveButton() {
        FeatureOverrides.overrideFlag(
                OmniboxFeatureList.COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION, true);
        doReturn(false).when(mDriveFilePickerClient).isAvailable(any(), any());
        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedInputTypes(InputType.INPUT_TYPE_DRIVE_VALUE));
        mMediator.onPlusButtonClicked();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_DRIVE_VISIBLE));
    }

    @Test
    public void onInputStateChange_canvasDisablesTabs_whenFlagEnabled() {
        FeatureOverrides.overrideFlag(OmniboxFeatureList.OMNIBOX_DISABLE_TABS_FOR_CANVAS, true);
        recreateMediator();

        mInput.setRequestType(AutocompleteRequestType.CANVAS);
        mMediator.onPlusButtonClicked();

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onPlusButtonClicked();

        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
    }

    @Test
    public void onInputStateChange_canvasDoesNotDisableTabs_whenFlagDisabled() {
        FeatureOverrides.overrideFlag(OmniboxFeatureList.OMNIBOX_DISABLE_TABS_FOR_CANVAS, false);
        recreateMediator();

        mInput.setRequestType(AutocompleteRequestType.CANVAS);
        mMediator.onPlusButtonClicked();

        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        assertTrue(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
    }

    @Test
    public void onInputStateChange_tabsDisableCanvas_whenFlagEnabled() {
        FeatureOverrides.overrideFlag(OmniboxFeatureList.OMNIBOX_DISABLE_TABS_FOR_CANVAS, true);
        recreateMediator();

        FuseboxAttachment attachment =
                addAttachment("Tab Title", "token", FuseboxAttachmentType.ATTACHMENT_TAB);
        mMediator.onPlusButtonClicked();

        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));

        mMediator.onPlusButtonClicked();
        mAttachments.remove(attachment, /* isFailure= */ false);
        mMediator.onPlusButtonClicked();

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));
    }

    @Test
    public void onInputStateChange_tabsDoNotDisableCanvas_whenFlagDisabled() {
        FeatureOverrides.overrideFlag(OmniboxFeatureList.OMNIBOX_DISABLE_TABS_FOR_CANVAS, false);
        recreateMediator();

        addAttachment("Tab Title", "token", FuseboxAttachmentType.ATTACHMENT_TAB);
        mMediator.onPlusButtonClicked();

        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_CANVAS));
    }

    @Test
    public void onInputStateChange_updatesHeaders() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withToolsSectionConfig(
                                SectionConfig.newBuilder().setHeader("Tools Header").build())
                        .withModelSectionConfig(
                                SectionConfig.newBuilder().setHeader("Models Header").build()));
        mMediator.onPlusButtonClicked();

        assertEquals("Tools Header", mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_TEXT));
        assertEquals("Models Header", mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_TEXT));
    }

    @Test
    public void onInputStateChanged_setsCreateImageVisibilityAndEnablement() {

        setInputState(createDefaultInputStateBuilder().withAllowedTools());
        mMediator.onPlusButtonClicked();
        assertFalse(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        assertFalse(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));

        setInputState(
                createDefaultInputStateBuilder().withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        assertTrue(isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        assertTrue(isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
    }

    @Test
    public void onAutocompleteRequestTypeChanged_resetsActiveModel() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG, AUTO_MODEL_CONFIG));
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        assertEquals("Pro", models.get(0).text);
        assertEquals("Auto", models.get(1).text);
        models.get(1).onClicked.run();

        verify(mComposeboxQueryControllerBridge)
                .setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE);
        assertEquals(AutocompleteRequestType.AI_MODE, mInput.getRequestType());
        clearInvocations(mComposeboxQueryControllerBridge);

        // The active model should be reset to the default (Pro).
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        verify(mComposeboxQueryControllerBridge).setActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO);
        assertEquals(AutocompleteRequestType.SEARCH, mInput.getRequestType());
    }

    @Test
    public void testReconcileSuggestedTabs() {
        mMediator.beginInput(mSession);

        SuggestedTabInfo info =
                new SuggestedTabInfo(1, "Title", new GURL("https://google.com"), 12345L);
        when(mTab.getId()).thenReturn(1);
        when(mTab.getTitle()).thenReturn("Title");
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
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
    public void testReconcileSuggestedTabs_doesNotQueueLoad() {
        mMediator.beginInput(mSession);
        SuggestedTabInfo info =
                new SuggestedTabInfo(1, "Title", new GURL("https://google.com"), 12345L);
        when(mTab.getId()).thenReturn(1);
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);

        mSuggestedTabsSupplier.set(List.of(info));
        RobolectricUtil.runAllBackgroundAndUi();

        // Suggested tab loads are never cancelled, so they must never be queued either.
        verify(mTabLoadingService, never()).queueLoadIfNeeded(any());
    }

    @Test
    public void updateModelForRecentTabs_nonDesktop_remainsHidden() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE));
    }

    @Test
    public void updateModelForRecentTabs_desktopPlatform_populatesRecentTabs() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);

        // Active tab.
        Tab tab1 = mockTab(1, JUnitTestGURLs.GOOGLE_URL);
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab1);

        // Recent tabs.
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        mTabMap.put(2, tab2);

        mockTab(3, JUnitTestGURLs.URL_1);
        mockTab(4, JUnitTestGURLs.URL_2);

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();

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

        assertFalse(mMediator.wasPopupItemSelected());
        recentTabs.get(0).onClicked.run();
        assertTrue(mMediator.wasPopupItemSelected());
        assertEquals(1, mAttachments.size());
        assertEquals(4, mAttachments.get(0).getTabId());
        histogramWatcher.assertExpected();
    }

    @Test
    public void cameraButtonVisibility_desktopPlatform() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();

        assertFalse(mModel.get(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE));
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
    public void onInputStateChanged_setsRequestTypeButtonText() {

        setInputState(createDefaultInputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_CANVAS));
        assertEquals("Canvas", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        setInputState(
                createDefaultInputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_UNSPECIFIED));
        assertEquals("AI Mode", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
    }

    @Test
    public void testOnInputStateChange_desktopPlatform() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);

        setInputState(
                createDefaultInputStateBuilder().withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH));
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> tools = mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        assertEquals(1, tools.size());
        assertEquals("Deep Search", tools.get(0).text);
        assertFalse(isToolVisible(ToolMode.TOOL_MODE_UNSPECIFIED));
    }

    @Test
    public void testOnInputStateChange_toolAndModelTooltip() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        recreateMediator();

        ToolConfig toolNoTooltip = CANVAS_TOOL_CONFIG.toBuilder().clearMenuTooltip().build();
        ModelConfig modelNoTooltip = PRO_MODEL_CONFIG.toBuilder().clearMenuTooltip().build();

        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH, ToolMode.TOOL_MODE_CANVAS)
                        .withToolConfigs(DEEP_SEARCH_TOOL_CONFIG, toolNoTooltip)
                        .withModelConfigs(AUTO_MODEL_CONFIG, modelNoTooltip));
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> tools = mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        assertEquals(2, tools.size());
        assertEquals("Research a topic in depth", tools.get(0).tooltip);
        assertEquals("", tools.get(1).tooltip);

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(2, models.size());
        assertEquals(
                "Intelligently routes to the best model for your request", models.get(0).tooltip);
        assertEquals("", models.get(1).tooltip);
    }

    @Test
    public void testHandleKeyEvent() {
        addAttachment("title", "token", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        addAttachment("title2", "token2", FuseboxAttachmentType.ATTACHMENT_IMAGE);
        assertEquals(2, mAttachments.size());

        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        // Test Forward Tab
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();

        // Initial state: select first attachment
        mMediator.selectFirstAttachment();
        assertTrue(
                mAttachments.get(0).model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));

        // Press TAB -> should move to second attachment
        assertTrue(mMediator.handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertFalse(
                mAttachments.get(0).model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));
        assertTrue(
                mAttachments.get(1).model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));

        // Test Backward Tab
        doReturn(false).when(mKeyEvent).hasNoModifiers();
        doReturn(true).when(mKeyEvent).hasModifiers(KeyEvent.META_SHIFT_ON);

        mMediator.selectLastAttachment();
        assertTrue(mMediator.handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertTrue(
                mAttachments.get(0).model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));

        // Test Activation
        doReturn(KeyEvent.KEYCODE_ENTER).when(mKeyEvent).getKeyCode();

        // Setup ON_REMOVE runnable to verify activation
        mAttachments.get(0).model.set(FuseboxAttachmentProperties.ON_REMOVE, mOnRemoveRunnable);

        mMediator.selectFirstAttachment();
        assertTrue(mMediator.handleKeyEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent));
        verify(mOnRemoveRunnable).run();
    }

    @Test
    public void testOnInputStateChange_lazyUntilPopupShown() {

        setInputState(
                createDefaultInputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_DEEP_SEARCH));

        // Request type button text is updated eagerly for the toolbar.
        assertEquals("Deep Search", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        // Popup properties are NOT populated while the popup is hidden.
        List<PopupButtonData> initialTools =
                mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        List<PopupButtonData> initialModels =
                mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertTrue(initialTools == null || initialTools.isEmpty());
        assertTrue(initialModels == null || initialModels.isEmpty());

        // Opening the popup lazily populates popup button data.
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> tools = mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertNotNull(tools);
        assertNotNull(models);
        assertFalse(tools.isEmpty());
        assertEquals(2, models.size());
    }

    @Test
    public void testOnInputStateChange_unknownIconResourceIds() {

        int unknownIconId = 9999;
        ModelConfig configAuto =
                AUTO_MODEL_CONFIG.toBuilder()
                        .setIcon(Icon.newBuilder().setIconIdValue(unknownIconId))
                        .build();
        ToolConfig deepSearchConfig =
                DEEP_SEARCH_TOOL_CONFIG.toBuilder()
                        .setIcon(Icon.newBuilder().setIconIdValue(unknownIconId))
                        .build();

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withToolConfigs(deepSearchConfig)
                        .withModelConfigs(configAuto, PRO_MODEL_CONFIG));
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> tools = mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        assertEquals(unknownIconId, tools.get(1).iconId);
        assertEquals(unknownIconId, models.get(0).iconId);
    }

    @Test
    public void testActivateSearchMode_deduplicatesSetActiveModel() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withAllowedModels(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG));

        // Switch to AI mode via request type button.
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(AutocompleteRequestType.AI_MODE, mModel.get(FuseboxProperties.REQUEST_TYPE));
        clearInvocations(mComposeboxQueryControllerBridge);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.SET_ACTIVE_MODEL_SOURCE_HISTOGRAM,
                        SetActiveModelSource.SKIPPED_FROM_ACTIVATE_SEARCH);

        // Switch back to search mode. Since active model is already default, setActiveModel is
        // skipped.
        mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run();
        assertEquals(AutocompleteRequestType.SEARCH, mModel.get(FuseboxProperties.REQUEST_TYPE));
        verify(mComposeboxQueryControllerBridge, never()).setActiveModel(anyInt());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSetModelMode_recordsHistogram() {

        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withModelConfigs(PRO_MODEL_CONFIG, AUTO_MODEL_CONFIG));
        mMediator.onPlusButtonClicked();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.SET_ACTIVE_MODEL_SOURCE_HISTOGRAM,
                        SetActiveModelSource.SET_FROM_MODEL_SELECTION);

        List<PopupButtonData> models = mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        models.get(1).onClicked.run();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnInputStateChange_deduplicatesRequestTypeButtonText() {

        setInputState(createDefaultInputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_CANVAS));
        assertEquals("Canvas", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));

        mModel.addObserver(mPropertyObserver);

        // Emit another InputState with the same active tool / button text.
        setInputState(createDefaultInputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_CANVAS));

        // Should not notify observer since button text has not changed.
        verify(mPropertyObserver, never())
                .onPropertyChanged(eq(mModel), eq(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
    }

    @Test
    public void testPopupItemSelected_recentTab_setsPopupItemSelected() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);

        Tab tab1 = mockTab(1, JUnitTestGURLs.GOOGLE_URL);
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab1);
        mockTab(2, JUnitTestGURLs.URL_1);

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();

        assertFalse(mMediator.wasPopupItemSelected());
        List<PopupButtonData> recentTabs =
                mModel.get(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST);
        recentTabs.get(0).onClicked.run();

        assertTrue(mMediator.wasPopupItemSelected());
    }

    @Test
    public void testPopupItemSelected_currentTab_setsPopupItemSelected() {
        OmniboxFeatures.sAllowCurrentTab.setForTesting(/* overrideValue= */ true);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn("Title1").when(mTab1).getTitle();
        doReturn(new GURL("https://www.google.com")).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isInitialized();
        doReturn(mWebContents).when(mTab1).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
        doReturn("token").when(mComposeboxQueryControllerBridge).addTabContext(mTab1, false);

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();

        assertFalse(mMediator.wasPopupItemSelected());
        mModel.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run();

        assertTrue(mMediator.wasPopupItemSelected());
    }

    @Test
    public void testPopupItemSelected_tabPickerResult_setsPopupItemSelected() {
        assertFalse(mMediator.wasPopupItemSelected());

        mockTab(101, /* webContentsReady= */ true);
        ArrayList<Integer> selectedTabIds = new ArrayList<>(Arrays.asList(101));
        Intent resultIntent = createTabPickerResultIntent(selectedTabIds);

        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);

        assertTrue(mMediator.wasPopupItemSelected());
    }

    @Test
    public void testPopupItemSelected_beginInput_resetsPopupItemSelected() {
        mockTab(101, /* webContentsReady= */ true);
        ArrayList<Integer> selectedTabIds = new ArrayList<>(Arrays.asList(101));
        Intent resultIntent = createTabPickerResultIntent(selectedTabIds);
        mMediator.onTabPickerResult(Activity.RESULT_OK, resultIntent);
        assertTrue(mMediator.wasPopupItemSelected());

        // Beginning a new input session resets it.
        mMediator.beginInput(mSession);
        assertFalse(mMediator.wasPopupItemSelected());
    }

    @Test
    public void moreOptionsClicked_togglesAccordionExpanded() {
        OmniboxFeatures.setUseAccordionForTesting(true);
        recreateMediator();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));

        var watcher1 =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AccordionToggled", true);
        mModel.get(FuseboxProperties.POPUP_MORE_OPTIONS_CLICKED).run();
        assertTrue(mModel.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));
        watcher1.assertExpected();

        var watcher2 =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AccordionToggled", false);
        mModel.get(FuseboxProperties.POPUP_MORE_OPTIONS_CLICKED).run();
        assertFalse(mModel.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));
        watcher2.assertExpected();
    }

    @Test
    public void getToolSubtext_returnsLocalizedStrings() {
        OmniboxFeatures.setUseAccordionForTesting(true);
        assertEquals(
                mContext.getString(R.string.fusebox_ai_mode_subtext),
                mMediator.getToolSubtext(ToolMode.TOOL_MODE_UNSPECIFIED));
        assertEquals(
                mContext.getString(R.string.fusebox_create_image_subtext),
                mMediator.getToolSubtext(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertEquals(
                mContext.getString(R.string.fusebox_canvas_subtext),
                mMediator.getToolSubtext(ToolMode.TOOL_MODE_CANVAS));
        assertEquals("", mMediator.getToolSubtext(ToolMode.TOOL_MODE_DEEP_SEARCH));

        OmniboxFeatures.setUseAccordionForTesting(false);
        assertEquals("", mMediator.getToolSubtext(ToolMode.TOOL_MODE_UNSPECIFIED));
        assertEquals("", mMediator.getToolSubtext(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertEquals("", mMediator.getToolSubtext(ToolMode.TOOL_MODE_CANVAS));
        assertEquals("", mMediator.getToolSubtext(ToolMode.TOOL_MODE_DEEP_SEARCH));
    }

    @Test
    public void popupHeaders_accordionEnabled_suppressed() {
        OmniboxFeatures.setUseAccordionForTesting(true);
        recreateMediator();
        mMediator.onPlusButtonClicked();

        assertEquals(4, mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST).size());
        assertEquals(2, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertFalse(mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE));
        assertFalse(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void popupHeaders_accordionDisabled_visible() {
        OmniboxFeatures.setUseAccordionForTesting(false);
        recreateMediator();
        mMediator.onPlusButtonClicked();

        assertEquals(4, mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST).size());
        assertEquals(2, mModel.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST).size());
        assertTrue(mModel.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE));
        assertTrue(mModel.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE));
    }

    @Test
    public void testOnInputStateChange_activeToolDeepSearch() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        ToolConfig config =
                ToolConfig.newBuilder()
                        .setToolValue(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setChipLabel("Deep Search Chip")
                        .setIcon(
                                Icon.newBuilder().setIconId(IconResourceIds.TRAVEL_EXPLORE).build())
                        .build();
        InputState state =
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withToolConfigs(new byte[][] {config.toByteArray()})
                        .build();

        mInputStateSupplier.set(state);

        assertEquals(
                "Deep Search Chip",
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.TRAVEL_EXPLORE,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
        assertEquals("Deep Search Chip", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON));
    }

    @Test
    public void testOnInputStateChange_activeToolImageGen() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        ToolConfig config =
                ToolConfig.newBuilder()
                        .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setChipLabel("Create Image Chip")
                        .setIcon(Icon.newBuilder().setIconId(IconResourceIds.BANANA).build())
                        .build();
        InputState state =
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withToolConfigs(new byte[][] {config.toByteArray()})
                        .build();

        mInputStateSupplier.set(state);

        assertEquals(
                "Create Image Chip",
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.BANANA,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
        assertEquals("Create Image Chip", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
        assertFalse(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON));
    }

    @Test
    public void testOnInputStateChange_activeToolImageGen_imageCreate() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        ToolConfig config =
                ToolConfig.newBuilder()
                        .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setChipLabel("Create Image Chip")
                        .setIcon(Icon.newBuilder().setIconId(IconResourceIds.IMAGE_CREATE).build())
                        .build();
        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withToolConfigs(config));

        assertEquals(
                "Create Image Chip",
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.IMAGE_CREATE,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
        assertEquals("Create Image Chip", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON));
    }

    @Test
    public void testPopupToolButton_hasColor() {
        setInputState(
                createDefaultInputStateBuilder()
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_IMAGE_GEN, ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        mMediator.onPlusButtonClicked();

        List<PopupButtonData> tools = mModel.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        assertEquals(3, tools.size());
        assertFalse(tools.get(0).hasColor);
        assertTrue(tools.get(1).hasColor);
        assertEquals(IconResourceIds.BANANA_VALUE, tools.get(1).iconId);
        assertFalse(tools.get(2).hasColor);
        assertEquals(IconResourceIds.IMAGE_CREATE_VALUE, tools.get(2).iconId);
    }

    @Test
    public void testOnInputStateChange_activeToolUnspecified() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        InputState state =
                new InputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_UNSPECIFIED).build();

        mInputStateSupplier.set(state);

        assertEquals(
                mContext.getString(R.string.acc_send_button_send_to_ai),
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
        assertEquals("AI Mode", mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT));
        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON));
    }

    @Test
    public void testOnInputStateChange_searchRequestType() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        InputState state =
                new InputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_UNSPECIFIED).build();
        mInputStateSupplier.set(state);

        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        assertEquals(
                mContext.getString(R.string.acc_send_button_search_or_navigate),
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
    }

    @Test
    public void testOnInputStateChange_activeTool_fallback() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        InputState state =
                new InputStateBuilder().withActiveTool(ToolMode.TOOL_MODE_DEEP_SEARCH).build();

        mInputStateSupplier.set(state);

        assertEquals(
                mContext.getString(R.string.acc_send_button_send_to_ai),
                mModel.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
        assertTrue(mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON));
    }

    @Test
    public void testOnInputStateChange_activeTool_unspecifiedIconFallback() {
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
        ToolConfig config =
                ToolConfig.newBuilder()
                        .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setIcon(
                                Icon.newBuilder()
                                        .setIconIdValue(
                                                FuseboxMediator.UNSPECIFIED_ICON_RESOURCE_ID))
                        .build();
        setInputState(
                createDefaultInputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withToolConfigs(config));

        assertEquals(
                IconResourceIdsProtoIntDef.IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE,
                mModel.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE)
    public void onAddRecentTab_inactiveTab_queuesLoadViaTabLoadingService() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();
        Tab inactiveTab = mockTab(2, JUnitTestGURLs.URL_1);
        when(inactiveTab.getWebContents()).thenReturn(null);

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST).get(0).onClicked.run();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mTabLoadingService).queueLoadIfNeeded(inactiveTab);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE,
        ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                + ":cancel_load_on_deselection/true"
    })
    public void onAddRecentTab_cancelledTab_queuesLoadViaTabLoadingService() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        recreateMediator();
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);
        Tab cancelledTab = mockTab(2, JUnitTestGURLs.URL_1);
        when(cancelledTab.needsReload()).thenReturn(true);

        mModel.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run();
        mModel.get(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST).get(0).onClicked.run();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mTabLoadingService).queueLoadIfNeeded(cancelledTab);
    }
}
