// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.Pair;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.util.List;

/** Unit tests for the {@link AdaptiveToolbarButtonController} */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
public class AdaptiveToolbarButtonControllerTest {

    @Mock private AndroidPermissionDelegate mAndroidPermissionDelegate;
    @Mock private ButtonDataProvider mShareButtonController;
    @Mock private ButtonDataProvider mVoiceToolbarButtonController;
    @Mock private ButtonDataProvider mNewTabButtonController;
    @Mock private ButtonDataProvider mPriceTrackingButtonController;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private Configuration mConfiguration;

    private ButtonDataImpl mButtonData;
    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mButtonData =
                new ButtonDataImpl(
                        /* canShow= */ true,
                        /* drawable= */ null,
                        mock(View.OnClickListener.class),
                        /* contentDescription= */ "",
                        /* supportsTinting= */ false,
                        /* iphCommandBuilder= */ null,
                        /* isEnabled= */ true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ false);
        mConfiguration.screenWidthDp = 420;
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        mProfileSupplier = new ObservableSupplierImpl<>();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        ChromeSharedPreferences.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCustomization_newTab() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.NEW_TAB)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mNewTabButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCustomization_share() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.SHARE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mShareButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCustomization_voice() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCustomization_prefChangeTriggersButtonChange() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                        AdaptiveToolbarButtonVariant.NEW_TAB);

        verify(observer, times(2)).buttonDataChanged(true);
        Assert.assertEquals(
                mNewTabButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testLongPress() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.NEW_TAB)));
        Activity activity = Robolectric.setupActivity(Activity.class);

        AdaptiveButtonActionMenuCoordinator menuCoordinator =
                mock(AdaptiveButtonActionMenuCoordinator.class);
        Answer<OnLongClickListener> listenerAnswer =
                invocation ->
                        (view -> {
                            invocation
                                    .<Callback<Integer>>getArgument(0)
                                    .onResult(
                                            Integer.valueOf(
                                                    R.id.customize_adaptive_button_menu_id));
                            return true;
                        });
        doAnswer(listenerAnswer).when(menuCoordinator).createOnLongClickListener(any());

        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(
                        activity,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier,
                        menuCoordinator,
                        mAndroidPermissionDelegate);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.NEW_TAB, mNewTabButtonController);
        mProfileSupplier.set(mProfile);

        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.NEW_TAB));
        when(mNewTabButtonController.get(any())).thenReturn(mButtonData);
        View view = mock(View.class);
        when(view.getContext()).thenReturn(activity);

        View.OnLongClickListener longClickListener =
                adaptiveToolbarButtonController.get(mTab).getButtonSpec().getOnLongClickListener();
        longClickListener.onLongClick(view);
        adaptiveToolbarButtonController.destroy();

        verify(mSettingsNavigation).startSettings(activity, AdaptiveToolbarSettingsFragment.class);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testShowDynamicAction() {
        Activity activity = Robolectric.setupActivity(Activity.class);

        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.NEW_TAB)));

        AdaptiveButtonActionMenuCoordinator menuCoordinator =
                mock(AdaptiveButtonActionMenuCoordinator.class);

        doReturn(
                        new OnLongClickListener() {
                            @Override
                            public boolean onLongClick(View view) {
                                Assert.fail("This long click listener shouldn't be invoked.");
                                return false;
                            }
                        })
                .when(menuCoordinator)
                .createOnLongClickListener(any());

        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(
                        activity,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier,
                        menuCoordinator,
                        mAndroidPermissionDelegate);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, mPriceTrackingButtonController);
        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.PRICE_TRACKING));
        when(mPriceTrackingButtonController.get(any())).thenReturn(mButtonData);
        View view = mock(View.class);
        when(view.getContext()).thenReturn(activity);

        adaptiveToolbarButtonController.showDynamicAction(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        // Button data should have change twice, first on native initialization and then after
        // showing the dynamic action.
        verify(observer, times(2)).buttonDataChanged(true);
        Assert.assertEquals(
                mPriceTrackingButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        ButtonSpec buttonSpec = adaptiveToolbarButtonController.get(mTab).getButtonSpec();
        Assert.assertEquals(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, buttonSpec.getButtonVariant());
        Assert.assertTrue(buttonSpec.isDynamicAction());
        // Dynamic actions should have no long click handlers.
        Assert.assertNull(buttonSpec.getOnLongClickListener());
        adaptiveToolbarButtonController.destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testButtonOnLargeScreens() {
        // Screen is wide enough to fit the button, it should appear.
        mConfiguration.screenWidthDp = 450;
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        Assert.assertTrue(adaptiveToolbarButtonController.get(mTab).canShow());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testButtonNotShownOnSmallScreens() {
        // Screen too narrow, button shouldn't appear.
        mConfiguration.screenWidthDp = 320;
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        Assert.assertFalse(adaptiveToolbarButtonController.get(mTab).canShow());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testButtonVisibilityChangeOnConfigurationChange() {
        // Screen too narrow, button shouldn't appear.
        mConfiguration.screenWidthDp = 320;
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);
        doReturn(true).when(mActivityLifecycleDispatcher).isNativeInitializationFinished();

        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        mProfileSupplier.set(mProfile);

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(
                mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        Assert.assertFalse(adaptiveToolbarButtonController.get(mTab).canShow());

        // New screen configuration is wider, button should be visible.
        mConfiguration.screenWidthDp = 450;

        adaptiveToolbarButtonController.onConfigurationChanged(mConfiguration);

        verify(observer).buttonDataChanged(false);
        Assert.assertTrue(adaptiveToolbarButtonController.get(mTab).canShow());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testConfigurationChangeIgnoredWhenNativeNotReady() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, List.of(AdaptiveToolbarButtonVariant.VOICE)));
        // If native is not done initializing then ignore all configuration changes.
        doReturn(false).when(mActivityLifecycleDispatcher).isNativeInitializationFinished();

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);

        // Change configuration, button shouldn't update.
        mConfiguration.screenWidthDp = 320;
        adaptiveToolbarButtonController.onConfigurationChanged(mConfiguration);

        verify(observer, never()).buttonDataChanged(true);
    }

    private AdaptiveToolbarButtonController buildController() {
        Activity mockActivity = mock(Activity.class);
        Resources mockResources = mock(Resources.class);
        doReturn(mockResources).when(mockActivity).getResources();
        doReturn(mConfiguration).when(mockResources).getConfiguration();

        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(
                        mockActivity,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier,
                        mock(AdaptiveButtonActionMenuCoordinator.class),
                        mAndroidPermissionDelegate);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.NEW_TAB, mNewTabButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.SHARE, mShareButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.VOICE, mVoiceToolbarButtonController);
        return adaptiveToolbarButtonController;
    }

    private static ButtonSpec makeButtonSpec(@AdaptiveToolbarButtonVariant int variant) {
        return new ButtonSpec(
                /* drawable= */ null,
                mock(View.OnClickListener.class),
                /* onLongClickListener= */ null,
                /* contentDescription= */ "description",
                /* supportsTinting= */ false,
                /* iphCommandBuilder= */ null,
                variant,
                /* actionChipLabelResId= */ 0,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);
    }
}
