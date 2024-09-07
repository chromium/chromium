// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
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
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
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

    @Mock TemplateUrl mTemplateUrl;

    @Mock Callback<LoadUrlParams> mLogoClickedCallback;

    @Mock Callback<Logo> mOnLogoAvailableCallback;

    @Captor
    private ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver>
            mTemplateUrlServiceObserverArgumentCaptor;

    private Context mContext;
    private PropertyModel mLogoModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = ApplicationProvider.getApplicationContext();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(mTemplateUrl);
        when(mTemplateUrl.getKeyword()).thenReturn(null);

        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridgeJniMock);

        ThreadUtils.runOnUiThreadBlocking(
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

        logoMediator.updateVisibility(/* animationEnabled= */ false);

        verify(mLogoBridge, times(1)).getCurrentLogo(any());
    }

    @Test
    public void testLoadLogoWhenLogoHasLoaded() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);

        logoMediator.updateVisibility(/* animationEnabled= */ false);

        verify(mLogoBridge, times(0)).getCurrentLogo(any());
    }

    @Test
    public void testInitWithNativeWhenParentSurfaceIsVisible() {
        LogoMediator logoMediator = createMediatorWithoutNative(true);
        logoMediator.updateVisibility(/* animationEnabled= */ false);

        Assert.assertTrue(logoMediator.isLogoVisible());
        // When parent surface is shown while native library isn't loaded, calling
        // updateVisibilityAndMaybeCleanUp() will add a pending load task.
        Assert.assertTrue(logoMediator.getIsLoadPendingForTesting());
        logoMediator.initWithNative(mProfile);

        Assert.assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(1)).getCurrentLogo(any());
        verify(mTemplateUrlService).addObserver(logoMediator);
    }

    @Test
    public void testInitWithoutNativeWhenDseDoesNotHaveLogo() {
        LogoMediator logoMediator = createMediatorWithoutNative(true);
        boolean originKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO,
                                mTemplateUrlService.doesDefaultSearchEngineHaveLogo());
        ChromeSharedPreferences.getInstance()
                .writeBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, false);
        logoMediator.updateVisibility(/* animationEnabled= */ false);
        Assert.assertFalse(mLogoModel.get(LogoProperties.VISIBILITY));
        Assert.assertFalse(logoMediator.getIsLoadPendingForTesting());
        verify(mLogoBridge, times(0)).destroy();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, originKeyValue);
    }

    @Test
    public void testUpdateVisibility() {
        LogoMediator logoMediator = createMediator();

        // If parent surface is shown, logo should be loaded and bridge shouldn't be
        // destroyed.
        logoMediator.setLogoBridgeForTesting(mLogoBridge);
        logoMediator.updateVisibility(/* animationEnabled= */ false);
        Assert.assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge).getCurrentLogo(any());
        verify(mLogoBridge, never()).destroy();
        Assert.assertFalse(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));

        // Attached the test for animationEnabled.
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        logoMediator.updateVisibility(/* animationEnabled= */ true);
        Assert.assertTrue(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));
    }

    @Test
    public void testDestroyWhenInitWithNative() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setLogoBridgeForTesting(null);
        logoMediator.destroy();
        verify(mTemplateUrlService).removeObserver(logoMediator);
    }

    private LogoMediator createMediator(boolean shouldFetchDoodle) {
        LogoMediator logoMediator = createMediatorWithoutNative(shouldFetchDoodle);
        logoMediator.initWithNative(mProfile);
        return logoMediator;
    }

    private LogoMediator createMediator() {
        LogoMediator logoMediator = createMediatorWithoutNative(true);
        logoMediator.initWithNative(mProfile);
        return logoMediator;
    }

    private LogoMediator createMediatorWithoutNative(boolean shouldFetchDoodle) {
        LogoMediator logoMediator =
                new LogoMediator(
                        mContext,
                        mLogoClickedCallback,
                        mLogoModel,
                        shouldFetchDoodle,
                        mOnLogoAvailableCallback,
                        null,
                        new CachedTintedBitmap(
                                R.drawable.google_logo, R.color.google_logo_tint_color));
        logoMediator.setLogoBridgeForTesting(mLogoBridge);
        return logoMediator;
    }
}
