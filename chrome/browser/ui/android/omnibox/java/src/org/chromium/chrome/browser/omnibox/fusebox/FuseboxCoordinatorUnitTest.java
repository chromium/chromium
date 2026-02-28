// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.LayoutInflater;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.ViewportRectProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/** Unit tests for {@link FuseboxCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutocompleteController mAutocompleteController;
    @Mock private AutocompleteController.Natives mControllerJniMock;
    @Mock private ComposeboxQueryControllerBridge mComposebox;
    @Mock private FuseboxMediator mMediator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Bitmap mBitmap;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SnackbarManager mSnackbarManager;

    private AutocompleteInput mAutocompleteInput;

    private ActivityController<TestActivity> mActivityController;
    private WindowAndroid mWindowAndroid;
    private FuseboxCoordinator mCoordinator;

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createNonNull(mTabModelSelector);
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final Function<Tab, Bitmap> mTabFaviconFunction = (tab) -> mBitmap;
    private final List<Tab> mTabs = new ArrayList<>();

    @Before
    public void setUp() {
        AutocompleteControllerJni.setInstanceForTesting(mControllerJniMock);
        lenient().doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        mWindowAndroid = new WindowAndroid(activity, false);
        ConstraintLayout parent = new ConstraintLayout(activity);
        activity.setContentView(parent);
        LayoutInflater.from(activity).inflate(R.layout.fusebox_layout, parent, true);

        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFunction);

        lenient().doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        lenient().doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();

        doReturn(true).when(mIncognitoProfile).isIncognitoBranded();

        mAutocompleteInput =
                new AutocompleteInput()
                        .setPageClassification(
                                PageClassification
                                        .INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);

        mCoordinator =
                new FuseboxCoordinator(
                        activity,
                        mWindowAndroid,
                        parent,
                        mProfileSupplier,
                        mTabModelSelectorSupplier,
                        mTemplateUrlServiceSupplier,
                        mSnackbarManager);

        // By default, make the mediator available.
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.beginInput(createSession());
    }

    private FuseboxSessionState createSession() {
        return new FuseboxSessionState(mAutocompleteInput, mComposebox, null);
    }

    @After
    public void tearDown() {
        mActivityController.close();
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureEnabled_withBridge() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        mProfileSupplier.set(mProfile);
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureEnabled_noBridge() {
        // Start with a default state.
        mCoordinator.endInput();
        mComposebox = null;
        clearInvocations(mMediator);
        mProfileSupplier.set(mProfile);
        mCoordinator.beginInput(createSession());
        verify(mMediator, never()).beginInput(any());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureDisabled() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        mProfileSupplier.set(mProfile);
        assertNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_tracksProfileChanges() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        mProfileSupplier.set(mProfile);
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());

        mCoordinator.setMediatorForTesting(null);
        mProfileSupplier.set(mock(Profile.class));
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorNotInitialized() {
        // Contract is that Input is accepted only if Mediator is non-null, so
        // let's begin from a valid start state.
        mCoordinator.endInput();

        // Case where the Profile is not initialized, or the Bridge was not instantiated.
        mCoordinator.setMediatorForTesting(null);

        // Nothing should happen (including no crashes).
        mCoordinator.beginInput(createSession());
        mCoordinator.endInput();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorInitialized() {
        // setUp pre-initializes input for most tests, but this test verifies what this
        // step (beginInput) actually does, so let's reset to valid start state.
        mCoordinator.endInput();
        clearInvocations(mMediator);

        // Mediator set by setUp().
        mCoordinator.beginInput(createSession());
        verify(mMediator).beginInput(any());

        mCoordinator.endInput();
        verify(mMediator).endInput();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled() {
        // ViewHolder should be initialized as part of the init method.
        assertNotNull(mCoordinator.getViewHolderForTesting());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureDisabled() {
        // Nothing should get initialized.
        assertNull(mCoordinator.getViewHolderForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_basedOnPageClassification() {
        final Set<PageClassification> supportedPageClassifications =
                EnumSet.of(
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
                        PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                        PageClassification.OTHER);

        for (PageClassification pageClass : PageClassification.values()) {
            reset(mMediator);
            mAutocompleteInput.setPageClassification(pageClass.getNumber());

            mCoordinator.beginInput(createSession());

            boolean shouldBeVisible = supportedPageClassifications.contains(pageClass);
            verify(mMediator, times(shouldBeVisible ? 1 : 0)).beginInput(any());

            mCoordinator.endInput();
        }
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNonGoogleDse() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mCoordinator.beginInput(createSession());
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMediator).setToolbarVisible(false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNtpAiModeButtonPress() {
        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();
        mAutocompleteInput.setRequestType(AutocompleteRequestType.AI_MODE);

        mCoordinator.beginInput(createSession());
        verify(mMediator).beginInput(any());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void createImageButtonVisibility_regularProfile() {
        // Restart session with different eligibility.
        mCoordinator.endInput();
        doReturn(true).when(mComposebox).isCreateImagesEligible();
        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(false);
        mProfileSupplier.set(mProfile);
        mCoordinator.beginInput(createSession());

        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(true);
        mProfileSupplier.set(mIncognitoProfile);
        mProfileSupplier.set(mProfile);
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE));
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void testWrappingChange() {
        OmniboxFeatures.sCompactFusebox.setForTesting(true);
        mCoordinator.onFuseboxTextWrappingChanged(true);
        verify(mMediator).setUseCompactUi(false);

        mCoordinator.onFuseboxTextWrappingChanged(false);
        verify(mMediator).setUseCompactUi(true);

        mCoordinator.onFuseboxTextWrappingChanged(true);
        Mockito.clearInvocations(mMediator);

        mAutocompleteInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mCoordinator.onFuseboxTextWrappingChanged(false);
        verify(mMediator).setUseCompactUi(false);
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void viewportRectProvider() {
        Context context = mActivityController.get();
        ViewportRectProvider viewportRectProvider = new ViewportRectProvider(context);
        viewportRectProvider.onConfigurationChanged(new Configuration());
        int width = context.getResources().getDisplayMetrics().widthPixels;
        int height = context.getResources().getDisplayMetrics().heightPixels;
        assertEquals(new Rect(0, 0, width, height), viewportRectProvider.getRect());
    }
}
