// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for the {@link LogoMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoMediatorUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;

    @Mock LogoBridge.Natives mLogoBridgeJniMock;

    @Mock LogoBridge mLogoBridge;

    @Mock TemplateUrlService mTemplateUrlService;

    @Mock TemplateUrl mTemplateUrl;

    @Mock Callback<LoadUrlParams> mLogoClickedCallback;

    @Mock Callback<Logo> mOnLogoAvailableCallback;

    @Mock DoodleCache mDoodleCache;

    @Captor
    private ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver>
            mTemplateUrlServiceObserverArgumentCaptor;

    @Captor private ArgumentCaptor<LogoBridge.LogoObserver> mLogoObserverArgumentCaptor;

    private Context mContext;
    private PropertyModel mLogoModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(mTemplateUrl);
        when(mTemplateUrl.getKeyword()).thenReturn(null);

        DoodleCache.setInstanceForTesting(mDoodleCache);
        LogoBridgeJni.setInstanceForTesting(mLogoBridgeJniMock);

        ThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setJavaPrefHomepageEnabled(true));

        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
    }

    @After
    public void tearDown() {
        DoodleCache.setInstanceForTesting(null);
    }

    @Test
    public void testDseChangedAndGoogleIsDseAndDoodleIsSupported() {
        LogoMediator logoMediator = createMediator(mContext.getDrawable(R.drawable.ic_google_logo));
        Assert.assertNotNull(logoMediator.getDefaultGoogleLogoDrawable());

        verify(mTemplateUrlService)
                .addObserver(mTemplateUrlServiceObserverArgumentCaptor.capture());
        mTemplateUrlServiceObserverArgumentCaptor.getValue().onTemplateURLServiceChanged();

        verify(mLogoBridge, times(1)).getCurrentLogo(any());
    }

    @Test
    public void testDseChangedAndGoogleIsNotDse() {
        LogoMediator logoMediator = createMediator(mContext.getDrawable(R.drawable.ic_google_logo));
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        Assert.assertNull(logoMediator.getDefaultGoogleLogoDrawable());

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
    public void testLoadLogoUpdatesCache() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        Logo logo = mock(Logo.class);

        logoMediator.updateVisibility(/* animationEnabled= */ false);

        verify(mLogoBridge).getCurrentLogo(mLogoObserverArgumentCaptor.capture());
        mLogoObserverArgumentCaptor.getValue().onLogoAvailable(logo, false);

        verify(mDoodleCache).updateCachedDoodle(logo, null);
    }

    @Test
    public void testLoadLogoFromCache() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        Logo cachedLogo = mock(Logo.class);
        when(mDoodleCache.getCachedDoodle(any())).thenReturn(cachedLogo);

        logoMediator.updateVisibility(/* animationEnabled= */ true);

        // Should use cached logo and not call bridge
        verify(mLogoBridge, never()).getCurrentLogo(any());
        assertEquals(cachedLogo, mLogoModel.get(LogoProperties.LOGO));
        // Animation should be disabled when loading from cache
        Assert.assertFalse(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));
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
        LogoMediator logoMediator = createMediatorWithoutNative();
        logoMediator.updateVisibility(/* animationEnabled= */ false);

        assertTrue(logoMediator.isLogoVisible());
        // When parent surface is shown while native library isn't loaded, calling
        // updateVisibilityAndMaybeCleanUp() will add a pending load task.
        assertTrue(logoMediator.getIsLoadPendingForTesting());
        logoMediator.initWithNative(mProfile);

        assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge, times(1)).getCurrentLogo(any());
        verify(mTemplateUrlService).addObserver(logoMediator);
    }

    @SuppressWarnings("DirectInvocationOnMock")
    @Test
    public void testInitWithoutNativeWhenDseDoesNotHaveLogo() {
        LogoMediator logoMediator = createMediatorWithoutNative();
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
        assertTrue(logoMediator.isLogoVisible());
        verify(mLogoBridge).getCurrentLogo(any());
        verify(mLogoBridge, never()).destroy();
        Assert.assertFalse(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));

        // Attached the test for animationEnabled.
        logoMediator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        logoMediator.updateVisibility(/* animationEnabled= */ true);
        assertTrue(mLogoModel.get(LogoProperties.ANIMATION_ENABLED));
    }

    @Test
    public void testDestroyWhenInitWithNative() {
        LogoMediator logoMediator = createMediator();
        logoMediator.setLogoBridgeForTesting(null);
        logoMediator.destroy();
        verify(mTemplateUrlService).removeObserver(logoMediator);
    }

    @Test
    public void testIsDefaultGoogleLogoShown() {
        LogoMediator logoMediator = createMediator();
        Logo logo = mock(Logo.class);

        logoMediator.setShouldShowLogoForTesting(true);
        mLogoModel.set(LogoProperties.VISIBILITY, true);
        mLogoModel.set(LogoProperties.LOGO, null);
        assertTrue(logoMediator.isDefaultGoogleLogoShown());

        logoMediator.setShouldShowLogoForTesting(false);
        Assert.assertFalse(logoMediator.isDefaultGoogleLogoShown());

        logoMediator.setShouldShowLogoForTesting(true);
        mLogoModel.set(LogoProperties.VISIBILITY, false);
        Assert.assertFalse(logoMediator.isDefaultGoogleLogoShown());

        logoMediator.setShouldShowLogoForTesting(true);
        mLogoModel.set(LogoProperties.VISIBILITY, true);
        mLogoModel.set(LogoProperties.LOGO, logo);
        Assert.assertFalse(logoMediator.isDefaultGoogleLogoShown());
    }

    @Test
    public void testUpdateDefaultGoogleLogo() {
        LogoMediator logoMediator = createMediator();
        Drawable drawable = mock(Drawable.class);
        logoMediator.updateDefaultGoogleLogo(drawable);

        assertEquals(drawable, logoMediator.getDefaultGoogleLogoDrawable());
        assertTrue(mLogoModel.get(LogoProperties.SHOW_DEFAULT_GOOGLE_LOGO));
    }

    private LogoMediator createMediator() {
        return createMediator(null);
    }

    private LogoMediator createMediator(@Nullable Drawable defaultGoogleLogoDrawable) {
        LogoMediator logoMediator = createMediatorWithoutNative(defaultGoogleLogoDrawable);
        logoMediator.initWithNative(mProfile);
        return logoMediator;
    }

    private LogoMediator createMediatorWithoutNative() {
        return createMediatorWithoutNative(null);
    }

    private LogoMediator createMediatorWithoutNative(@Nullable Drawable defaultGoogleLogoDrawable) {
        LogoMediator logoMediator =
                new LogoMediator(
                        mLogoClickedCallback,
                        mLogoModel,
                        mOnLogoAvailableCallback,
                        null,
                        defaultGoogleLogoDrawable);
        logoMediator.setLogoBridgeForTesting(mLogoBridge);
        return logoMediator;
    }
}
