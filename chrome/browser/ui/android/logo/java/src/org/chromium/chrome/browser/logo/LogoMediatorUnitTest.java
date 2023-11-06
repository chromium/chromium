// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for the {@link LogoMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoMediatorUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfile;

    @Mock LogoBridge.Natives mLogoBridgeJniMock;

    @Mock LogoBridge mLogoBridge;

    @Mock ImageFetcher mImageFetcher;

    @Mock TemplateUrlService mTemplateUrlService;

    @Mock Callback<LoadUrlParams> mLogoClickedCallback;

    @Mock Callback<Logo> mOnLogoAvailableCallback;

    @Mock Runnable mOnCachedLogoRevalidatedRunnable;

    @Captor
    private ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver>
            mTemplateUrlServiceObserverArgumentCaptor;

    private Context mContext;
    private PropertyModel mLogoModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);

        mContext = ApplicationProvider.getApplicationContext();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridgeJniMock);

        Assert.assertTrue(ChromeFeatureList.sStartSurfaceAndroid.isEnabled());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(true));

        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
    }

    @Test
    public void testDseChangedAndGoogleIsDseAndDoodleIsSupported() {
        LogoMediator logoMediator = createMediator();
        Assert.assertNotNull(logoMediator.getDefaultGoogleLogo(mContext));

        verify(mTemplateUrlService)
                .addObserver(mTemplateUrlServiceObserverArgumentCaptor.capture());
        mTemplateUrlServiceObserverArgumentCaptor.getValue().onTemplateURLServiceChanged();

        verify(mLogoBridge, times(1)).getCurrentLogo(any());
    }

    @Test
    public void testDseChangedAndGoogleIsDseAndDoodleIsNotSupported() {
        // If doodle isn't supported, getSearchProviderLogo() shouldn't be called by
        // onTemplateURLServiceChanged().
        LogoMediator logoMediator = createMediator(false);
        Assert.assertNotNull(logoMediator.getDefaultGoogleLogo(mContext));

        verify(mTemplateUrlService)
                .addObserver(mTemplateUrlServiceObserverArgumentCaptor.capture());
        mTemplateUrlServiceObserverArgumentCaptor.getValue().onTemplateURLServiceChanged();

        verify(mLogoBridge, times(0)).getCurrentLogo(any());
    }

    @Test
    public void testDseChangedAndGoogleIsNotDse() {
        LogoMediator logoMediator = createMediator();
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        Assert.assertNull(logoMediator.getDefaultGoogleLogo(mContext));

        verify(mTemplateUrlService)
                .addObserver(mTemplateUrlServiceObserverArgumentCaptor.capture());
        mTemplateUrlServiceObserverArgumentCaptor.getValue().onTemplateURLServiceChanged();

        verify(mLogoBridge, times(1)).getCurrentLogo(any());
    }

    @Test
    public void testDseChangedAndDoesNotHaveLogo() {
        createMediator();
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        verify(mTemplateUrlService)
                .addObserver(mTemplateUrlServiceObserverArgumentCaptor.capture());
        mTemplateUrlServiceObserverArgumentCaptor.getValue().onTemplateURLServiceChanged();

        verify(mLogoBridge, times(0)).getCurrentLogo(any());
    }

    @Test
    public void testLoadLogoWhenLogoNotLoaded() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);

        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);

        verify(mLogoBridge, times(1)).getCurrentLogo(any());
    }

    @Test
    public void testLoadLogoWhenLogoHasLoaded() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);

        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);

        verify(mLogoBridge, times(0)).getCurrentLogo(any());
    }

    @Test
    public void testInitWithNativeWhenParentSurfaceIsNotVisible() {
        LogoMediator logoMediator =
                createMediatorWithoutNative(/* isParentSurfaceShown= */ false, true);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ false,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);
        Assert.assertFalse(mLogoModel.get(LogoProperties.VISIBILITY));
        // When parent surface isn't showing, calling updateVisibilityAndMaybeCleanUp() shouldn't
        // trigger getSearchProviderLogo() nor add any pending load task.
        Assert.assertFalse(logoMediator.getIsLoadPendingForTesting());
        logoMediator.initWithNative();

        Assert.assertFalse(mLogoModel.get(LogoProperties.VISIBILITY));
        Assert.assertFalse(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(0)).getCurrentLogo(any());
        verify(mTemplateUrlService).addObserver(logoMediator);
    }

    @Test
    public void testInitWithNativeWhenParentSurfaceIsVisible() {
        LogoMediator logoMediator = createMediatorWithoutNative(true, true);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);

        Assert.assertTrue(logoMediator.isLogoVisible());
        // When parent surface is shown while native library isn't loaded, calling
        // updateVisibilityAndMaybeCleanUp() will add a pending load task.
        Assert.assertTrue(logoMediator.getIsLoadPendingForTesting());
        logoMediator.initWithNative();

        Assert.assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(1)).getCurrentLogo(any());
        verify(mTemplateUrlService).addObserver(logoMediator);
    }

    @Test
    public void testInitWithoutNativeWhenDseDoesNotHaveLogo() {
        LogoMediator logoMediator = createMediatorWithoutNative(true, true);
        boolean originKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO,
                                mTemplateUrlService.doesDefaultSearchEngineHaveLogo());
        ChromeSharedPreferences.getInstance()
                .writeBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, false);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);
        Assert.assertFalse(mLogoModel.get(LogoProperties.VISIBILITY));
        Assert.assertFalse(logoMediator.getIsLoadPendingForTesting());
        verify(mLogoBridge, times(0)).destroy();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, originKeyValue);
    }

    @Test
    public void testUpdateVisibilityAndMaybeCleanUp() {
        LogoMediator logoMediator = createMediator();

        // If parent surface is not shown nor bridge shouldn't be destroyed, logo shouldn't be
        // loaded and bridge isn't destroyed.
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ false,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);
        Assert.assertFalse(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(0)).getCurrentLogo(any());
        verify(mLogoBridge, times(0)).destroy();

        // If parent surface is not shown and bridge should be destroyed, logo should be
        // loaded and bridge is destroyed.
        logoMediator.setImageFetcherForTesting(mImageFetcher);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ false,
                /* shouldDestroyBridge= */ true,
                /* animationEnabled= */ false);
        Assert.assertFalse(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(0)).getCurrentLogo(any());
        verify(mLogoBridge, times(1)).destroy();
        verify(mImageFetcher, times(1)).destroy();
        Assert.assertNull(logoMediator.getLogoBridgeForTesting());
        Assert.assertNull(logoMediator.getImageFetcherForTesting());

        // If parent surface is shown, logo should be loaded and bridge shouldn't be
        // destroyed.
        logoMediator.setLogoBridgeForTesting(mLogoBridge);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ false);
        Assert.assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(1)).getCurrentLogo(any());
        verify(mLogoBridge, times(1)).destroy();
        Assert.assertFalse(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));

        // Attached the test for animationEnabled.
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ false,
                /* animationEnabled= */ true);
        Assert.assertTrue(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));
    }

    @Test(expected = AssertionError.class)
    public void testUpdateVisibilityAndMaybeCleanUpAssertionError() {
        LogoMediator logoMediator = createMediator();
        verify(mLogoBridge, times(0)).getCurrentLogo(any());

        // If parent surface is shown and bridge should be destroyed, an assertion error
        // should be thrown.
        logoMediator.updateVisibilityAndMaybeCleanUp(
                /* isParentSurfaceShown= */ true,
                /* shouldDestroyBridge= */ true,
                /* animationEnabled= */ false); // should throw an exception
    }

    @Test
    public void testDestroyWhenInitWithNative() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setLogoBridgeForTesting(null);
        logoMediator.destroy();
        verify(mTemplateUrlService).removeObserver(logoMediator);
    }

    private LogoMediator createMediator(boolean shouldFetchDoodle) {
        LogoMediator logoMediator =
                createMediatorWithoutNative(/* isParentSurfaceShown= */ true, shouldFetchDoodle);
        logoMediator.initWithNative();
        return logoMediator;
    }

    private LogoMediator createMediator() {
        LogoMediator logoMediator =
                createMediatorWithoutNative(/* isParentSurfaceShown= */ true, true);
        logoMediator.initWithNative();
        return logoMediator;
    }

    private LogoMediator createMediatorWithoutNative(
            boolean isParentSurfaceShown, boolean shouldFetchDoodle) {
        LogoMediator logoMediator =
                new LogoMediator(
                        mContext,
                        mLogoClickedCallback,
                        mLogoModel,
                        shouldFetchDoodle,
                        mOnLogoAvailableCallback,
                        mOnCachedLogoRevalidatedRunnable,
                        isParentSurfaceShown,
                        null,
                        new CachedTintedBitmap(
                                R.drawable.google_logo, R.color.google_logo_tint_color));
        logoMediator.setLogoBridgeForTesting(mLogoBridge);
        return logoMediator;
    }
}
