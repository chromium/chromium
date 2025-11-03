// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.ViewGroup;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
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
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ComposeBoxQueryControllerBridge.Natives mControllerMock;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock NavigationAttachmentsMediator mMediator;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabModel mTabModel;
    private @Mock Bitmap mBitmap;
    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private NavigationAttachmentsCoordinator mCoordinator;
    private ViewGroup mParent;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>(mTabModelSelector);
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final Function<Tab, Bitmap> mTabFaviconFunction = (tab) -> mBitmap;
    private final List<Tab> mTabs = new ArrayList<>();

    @Before
    public void setUp() {
        ComposeBoxQueryControllerBridgeJni.setInstanceForTesting(mControllerMock);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mWindowAndroid = new WindowAndroid(mActivity, false);
        mParent = new ConstraintLayout(mActivity);
        mActivity.setContentView(mParent);
        LayoutInflater.from(mActivity).inflate(R.layout.fusebox_layout, mParent, true);

        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFunction);

        lenient().doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        lenient().doReturn(new ArrayList<>(mTabs).iterator()).when(mTabModel).iterator();
        lenient()
                .doReturn(PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(anyInt());

        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mParent,
                        mProfileSupplier,
                        mLocationBarDataProvider,
                        mTabModelSelectorSupplier,
                        mTemplateUrlServiceSupplier);

        // By default, make the Mediator available.
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureEnabled_withBridge() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        doReturn(/* nativeInstance= */ 1L).when(mControllerMock).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureEnabled_noBridge() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        doReturn(/* nativeInstance= */ 0L).when(mControllerMock).init(any(Profile.class));
        mProfileSupplier.set(mProfile);
        assertNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testOnProfileAvailable_featureDisabled() {
        // Start with a default state.
        mCoordinator.setMediatorForTesting(null);

        mProfileSupplier.set(mProfile);
        verify(mControllerMock, never()).init(any());
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
            Mockito.reset(mMediator);
            doReturn(pageClass.getNumber())
                    .when(mLocationBarDataProvider)
                    .getPageClassification(anyInt());

            mCoordinator.onUrlFocusChange(true);

            boolean shouldBeVisible = supportedPageClassifications.contains(pageClass);
            Mockito.verify(mMediator).setToolbarVisible(shouldBeVisible);

            if (shouldBeVisible) {
                mCoordinator.onUrlFocusChange(false);
                Mockito.verify(mMediator).setToolbarVisible(false);
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
}
