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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.LayoutInflater;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/** Unit tests for {@link NavigationAttachmentsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutocompleteController mAutocompleteController;
    @Mock private AutocompleteController.Natives mControllerJniMock;
    @Mock private ComposeBoxQueryControllerBridge.Natives mComposeboxController;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private FuseboxMediator mMediator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Bitmap mBitmap;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;

    private ActivityController<TestActivity> mActivityController;
    private WindowAndroid mWindowAndroid;
    private NavigationAttachmentsCoordinator mCoordinator;

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
        ComposeBoxQueryControllerBridgeJni.setInstanceForTesting(mComposeboxController);

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

        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        activity,
                        mWindowAndroid,
                        parent,
                        mProfileSupplier,
                        mLocationBarDataProvider,
                        mTabModelSelectorSupplier,
                        mTemplateUrlServiceSupplier,
                        mAutocompleteRequestTypeSupplier);

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
    public void testOnProfileAvailable_featureEnabled_withBridge() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        doReturn(/* nativeInstance= */ 1L).when(mComposeboxController).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureEnabled_noBridge() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        doReturn(/* nativeInstance= */ 0L).when(mComposeboxController).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        assertNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureDisabled() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        mProfileSupplier.set(mProfile);
        verify(mComposeboxController, never()).init(any());
        assertNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorNotInitialized() {
        // Case where the Profile is not initialized, or the Bridge was not instantiated.
        mCoordinator.setMediatorForTesting(null);

        // Nothing should happen (including no crashes).
        mCoordinator.onUrlFocusChange(true);
        mCoordinator.onUrlFocusChange(false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorInitialized() {
        // Mediator set by setUp().

        mCoordinator.onUrlFocusChange(true);
        verify(mMediator).setToolbarVisible(true);

        mCoordinator.onUrlFocusChange(false);
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

            mCoordinator.onUrlFocusChange(true);

            boolean shouldBeVisible = supportedPageClassifications.contains(pageClass);
            verify(mMediator).setToolbarVisible(shouldBeVisible);

            if (shouldBeVisible) {
                mCoordinator.onUrlFocusChange(false);
                verify(mMediator).setToolbarVisible(false);
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
        doReturn(/* nativeInstance= */ 1L).when(mComposeboxController).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);

        mCoordinator.onUrlFocusChange(true);
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                (long) mCoordinator.getAutocompleteRequestTypeSupplier().get());
    }

    @Test
    public void getAttachmentTokens_returnsEmptyListWhenMediatorNotSet() {
        List<String> tokens = mCoordinator.getAttachmentTokens();
        assertNotNull(tokens);
        assertTrue(tokens.isEmpty());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void getAttachmentTokens_returnsEmptyListWhenMediatorHasNoAttachments() {
        doReturn(/* nativeInstance= */ 1L).when(mComposeboxController).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();

        List<String> tokens = mCoordinator.getAttachmentTokens();
        assertNotNull(tokens);
        assertTrue(tokens.isEmpty());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT})
    public void getAttachmentTokens_returnsTokensWhenMediatorHasAttachments() {
        doReturn(/* nativeInstance= */ 1L).when(mComposeboxController).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();

        // Mock mediator with attachment tokens.
        var mockMediator = mock(FuseboxMediator.class);
        var testTokens = java.util.Arrays.asList("token1", "token2");
        doReturn(testTokens).when(mockMediator).getAttachmentTokens();
        mCoordinator.setMediatorForTesting(mockMediator);

        var tokens = mCoordinator.getAttachmentTokens();
        assertNotNull(tokens);
        assertEquals(2, tokens.size());
        assertEquals("token1", tokens.get(0));
        assertEquals("token2", tokens.get(1));
    }
}
