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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.reset;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
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
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private FuseboxMediator mMediator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Bitmap mBitmap;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;

    private ActivityController<TestActivity> mActivityController;
    private WindowAndroid mWindowAndroid;
    private FuseboxCoordinator mCoordinator;

    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>(mTabModelSelector);
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final Function<Tab, Bitmap> mTabFaviconFunction = (tab) -> mBitmap;
    private final List<Tab> mTabs = new ArrayList<>();
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier =
                    new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);

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
        lenient()
                .doReturn(PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(anyBoolean());

        doReturn(true).when(mIncognitoProfile).isIncognitoBranded();
        doReturn(true).when(mMediator).isInInputSession();

        mCoordinator =
                new FuseboxCoordinator(
                        activity,
                        mWindowAndroid,
                        parent,
                        mLocationBarDataProvider,
                        mTabModelSelectorSupplier,
                        mTemplateUrlServiceSupplier,
                        mAutocompleteRequestTypeSupplier,
                        mSnackbarManager);

        // By default, make the mediator available.
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @After
    public void tearDown() {
        mActivityController.close();
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorInitialized() {
        startRegularSession();
        verify(mMediator).setToolbarVisible(true);

        doReturn(false).when(mMediator).isInInputSession();
        clearSession();
        verify(mMediator).setToolbarVisible(false);
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
            doReturn(pageClass.getNumber())
                    .when(mLocationBarDataProvider)
                    .getPageClassification(anyBoolean());

            startRegularSession();

            boolean shouldBeVisible = supportedPageClassifications.contains(pageClass);
            verify(mMediator).setToolbarVisible(shouldBeVisible);

            if (shouldBeVisible) {
                clearSession();
                verify(mMediator, Mockito.atLeast(1)).setToolbarVisible(false);
            }
        }
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNonGoogleDse() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        ShadowLooper.idleMainLooper();

        verify(mMediator).setToolbarVisible(false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNtpAiModeButtonPress() {
        startRegularSession();
        ShadowLooper.idleMainLooper();
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);

        startRegularSession();
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (long) mCoordinator.getAutocompleteRequestTypeSupplier().get());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void createImageButtonVisibility_isCreateImagesEligible() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();
        startIncognitoSession();
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));

        doReturn(false).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();
        startRegularSession();
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void createImageButtonVisibility_incognitoProfile() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(false);
        startIncognitoSession();
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));

        clearSession();

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(true);
        startRegularSession();
        startIncognitoSession();
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void createImageButtonVisibility_regularProfile() {
        doReturn(true).when(mComposeBoxQueryControllerBridge).isCreateImagesEligible();

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(false);
        startRegularSession();
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));

        clearSession();

        OmniboxFeatures.sShowImageGenerationButtonInIncognito.setForTesting(true);
        startRegularSession();
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE));
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

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
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

    private void startRegularSession() {
        doReturn(true).when(mMediator).isInInputSession();
        mCoordinator.setInputSession(
                new FuseboxInputSession(mProfile, mComposeBoxQueryControllerBridge));
    }

    private void startIncognitoSession() {
        doReturn(true).when(mMediator).isInInputSession();
        mCoordinator.setInputSession(
                new FuseboxInputSession(mIncognitoProfile, mComposeBoxQueryControllerBridge));
    }

    private void clearSession() {
        doReturn(false).when(mMediator).isInInputSession();
        mCoordinator.setInputSession(null);
    }
}
