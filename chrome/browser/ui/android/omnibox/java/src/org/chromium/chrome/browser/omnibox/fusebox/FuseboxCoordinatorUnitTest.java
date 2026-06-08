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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.window.layout.WindowMetricsCalculator;

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
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.ViewportRectProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/** Unit tests for {@link FuseboxCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class FuseboxCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutocompleteController mAutocompleteController;
    @Mock private ComposeboxQueryControllerBridge mComposebox;
    @Mock private FuseboxMediator mMediator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Bitmap mBitmap;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private FuseboxMetrics mMetrics;
    @Mock private RectProvider.Observer mRectProviderObserver;
    @Mock private BackPressManager mBackPressManager;

    private AutocompleteInput mAutocompleteInput;
    private ActivityController<TestActivity> mActivityController;
    private WindowAndroid mWindowAndroid;
    private FuseboxCoordinator mCoordinator;

    private final SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createNonNull(mTabModelSelector);
    private final SettableNonNullObservableSupplier<List<SuggestedTabInfo>> mSuggestedTabsSupplier =
            ObservableSuppliers.createNonNull(List.of());
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final Function<Tab, @Nullable Bitmap> mTabFaviconFunction = (tab) -> mBitmap;
    private final NullableObservableSupplier<GURL> mExactMatchUrlSupplier =
            ObservableSuppliers.alwaysNull();

    @Before
    public void setUp() {
        AutocompleteController.setInstanceForTesting(mAutocompleteController);

        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        mWindowAndroid = new WindowAndroid(activity, false);
        ConstraintLayout parent = new ConstraintLayout(activity);
        activity.setContentView(parent);
        LayoutInflater.from(activity).inflate(R.layout.fusebox_layout, parent, true);

        OmniboxResourceProvider.setTabFaviconFactory(mTabFaviconFunction);

        lenient().doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        lenient().doReturn(Collections.emptyIterator()).when(mTabModel).iterator();
        doReturn(true).when(mComposebox).isFuseboxEligible();
        doReturn(mSuggestedTabsSupplier).when(mComposebox).getSuggestedTabsSupplier();

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
                        mTabModelSelectorSupplier,
                        mTemplateUrlServiceSupplier,
                        mSnackbarManager,
                        /* scrimAnchorViewSupplier= */ () -> null,
                        mBackPressManager,
                        mExactMatchUrlSupplier,
                        /* onActivationChipClickedWithQuery= */ () -> {},
                        /* clearUrlBarTextRunnable= */ () -> {},
                        /* urlBarTextSupplier= */ () -> "");
    }

    private FuseboxSessionState createSession() {
        return createSession(mProfile);
    }

    private FuseboxSessionState createSession(Profile profile) {
        var session = mock(FuseboxSessionState.class);
        lenient().doReturn(profile).when(session).getProfile();
        lenient().doReturn(mAutocompleteController).when(session).getAutocompleteController();
        lenient().doReturn(mAutocompleteInput).when(session).getAutocompleteInput();
        lenient().doReturn(mComposebox).when(session).getComposeboxQueryControllerBridge();
        lenient().doReturn(mMetrics).when(session).getMetrics();
        return session;
    }

    @After
    public void tearDown() {
        mActivityController.close();
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testBeginInput_initializesMediator() {
        mCoordinator.beginInput(createSession(mProfile));
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNotNull(mCoordinator.getMediatorForTesting());
        assertNotEquals(mMediator, mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testBeginInput_featureEnabled_noBridge() {
        var session = createSession();
        doReturn(null).when(session).getComposeboxQueryControllerBridge();
        mCoordinator.beginInput(session);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mMediator, never()).beginInput(any());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testBeginInput_featureDisabled() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_mediatorInitialized() {
        mCoordinator.setMediatorForTesting(mMediator);

        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mMediator).beginInput(any());

        mCoordinator.endInput();
        verify(mMediator).endInput();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled_disabledByServer() {
        mCoordinator.setMediatorForTesting(mMediator);

        doReturn(false).when(mComposebox).isFuseboxEligible();
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // We never activate the Fusebox in this scenario
        verify(mMediator, never()).beginInput(any());
        // ... but we want to be sure we reset the Fusebox UI if the user jumped tabs.
        verify(mMediator).endInput();

        clearInvocations(mMediator);

        // Ensure we still call endInput in the event the fusebox was previously active when flag
        // state changed (e.g. user jumped tabs) to properly reset UI state.
        mCoordinator.endInput();
        verify(mMediator).endInput();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // ViewHolder should be initialized as part of the init method.
        assertNotNull(mCoordinator.getViewHolderForTesting());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureDisabled() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // Nothing should get initialized.
        assertNull(mCoordinator.getViewHolderForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_basedOnPageClassification() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mCoordinator.setMediatorForTesting(mMediator);
        final Set<PageClassification> supportedPageClassifications =
                EnumSet.of(
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
                        PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                        PageClassification.CO_BROWSING_COMPOSEBOX,
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
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mCoordinator.setMediatorForTesting(mMediator);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mCoordinator.beginInput(createSession());
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mMediator).endInput();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNtpAiModeButtonPress() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mCoordinator.setMediatorForTesting(mMediator);
        mAutocompleteInput.setRequestType(AutocompleteRequestType.AI_MODE);

        mCoordinator.beginInput(createSession());
        verify(mMediator).beginInput(any());
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void viewportRectProvider() {
        Activity activity = mActivityController.get();
        ViewportRectProvider viewportRectProvider = new ViewportRectProvider(activity);
        viewportRectProvider.startObserving(mRectProviderObserver);

        viewportRectProvider.onConfigurationChanged(new Configuration());
        var windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(activity);
        var bounds = windowMetrics.getBounds();
        assertEquals(
                new Rect(0, 0, bounds.width(), bounds.height()), viewportRectProvider.getRect());
        verify(mRectProviderObserver).onRectChanged();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNotifyOmniboxSessionEnded() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mCoordinator.notifyOmniboxSessionEnded(true);

        verify(mMetrics).notifyOmniboxSessionEnded(eq(true), anyInt(), anyInt());

        mCoordinator.endInput();
        clearInvocations(mMetrics);

        mCoordinator.beginInput(createSession());
        mCoordinator.notifyOmniboxSessionEnded(false);

        verify(mMetrics).notifyOmniboxSessionEnded(eq(false), anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testPopupDismissed() {
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mCoordinator.setMediatorForTesting(mMediator);
        var viewHolder = assumeNonNull(mCoordinator.getViewHolderForTesting());
        viewHolder.plusButton.setVisibility(View.VISIBLE);
        mCoordinator.onContextPopupDismissed();
        assertTrue(viewHolder.plusButton.isFocused());
    }

    @Test
    public void testResetToSearchMode() {
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.resetToSearchMode();
        verify(mMediator).activateSearchMode();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testDseChangedToNonGoogleDuringSession() {
        // Start with Google DSE.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.beginInput(createSession());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Verify session is active (beginInput was called on mediator).
        verify(mMediator).beginInput(any());

        // Now DSE changes to non-Google.
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mCoordinator.onTemplateURLServiceChanged();

        // Verify that resetToSearchMode() / activateSearchMode() was called first.
        verify(mMediator).activateSearchMode();
        // Verify that endInput() was called.
        verify(mMediator).endInput();
    }
}
