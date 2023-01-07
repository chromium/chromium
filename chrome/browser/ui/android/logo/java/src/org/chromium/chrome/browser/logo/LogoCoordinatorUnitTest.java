// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for the {@link LogoCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoCoordinatorUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Profile mProfile;

    @Mock
    LogoBridge.Natives mLogoBridge;

    @Mock
    TemplateUrlServiceFactory.Natives mTemplateUrlServiceFactory;

    @Mock
    TemplateUrlService mTemplateUrlService;

    @Mock
    LogoDelegateImpl mLogoDelegate;

    @Mock
    LogoView mLogoView;

    @Mock
    Callback<LoadUrlParams> mLogoClickedCallback;

    @Mock
    Callback<Logo> mOnLogoAvailableCallback;

    @Mock
    Runnable mOnCachedLogoRevalidatedRunnable;

    private Context mContext;
    private LogoCoordinator mLogoCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);

        mContext = ApplicationProvider.getApplicationContext();

        mJniMocker.mock(TemplateUrlServiceFactoryJni.TEST_HOOKS, mTemplateUrlServiceFactory);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mTemplateUrlService).doesDefaultSearchEngineHaveLogo();

        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);

        CachedFeatureFlags.setForTesting(ChromeFeatureList.START_SURFACE_ANDROID, true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(true));
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @Test
    public void testDSEChangedAndGoogleIsDSE() {
        createCoordinator();
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mLogoCoordinator.setShouldFetchDoodleForTesting(true);

        mLogoCoordinator.onTemplateURLServiceChanged();

        Assert.assertNotNull(LogoView.getDefaultGoogleLogo(mContext));
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());

        // If doodle isn't supported, getSearchProviderLogo() shouldn't be called by
        // onTemplateURLServiceChanged().
        mLogoCoordinator.setShouldFetchDoodleForTesting(false);

        mLogoCoordinator.onTemplateURLServiceChanged();

        Assert.assertNotNull(LogoView.getDefaultGoogleLogo(mContext));
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedAndGoogleIsNotDSE() {
        createCoordinator();
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoCoordinator.onTemplateURLServiceChanged();

        Assert.assertNull(LogoView.getDefaultGoogleLogo(mContext));
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedAndDoesNotHaveLogo() {
        createCoordinator();
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        mLogoCoordinator.onTemplateURLServiceChanged();

        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testLoadLogoWhenLogoNotLoaded() {
        createCoordinator();
        mLogoCoordinator.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ true, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);

        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testLoadLogoWhenLogoHasLoaded() {
        createCoordinator();
        mLogoCoordinator.setHasLogoLoadedForCurrentSearchEngineForTesting(true);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ true, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);

        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testInitWithNativeWhenParentSurfaceIsNotVisible() {
        createCoordinatorWithoutNative(/*isParentSurfaceShown=*/false);
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ false, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);

        verify(mLogoView, times(1)).setVisibility(View.GONE);
        // When parent surface isn't showing, calling maybeLoadSearchProviderLogo() shouldn't
        // trigger getSearchProviderLogo() nor add any pending load task.
        Assert.assertFalse(mLogoCoordinator.getIsLoadPendingForTesting());
        mLogoCoordinator.initWithNative();

        verify(mLogoView, times(2)).setVisibility(View.GONE);
        Assert.assertFalse(mLogoCoordinator.isLogoVisible());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
        verify(mTemplateUrlService).addObserver(mLogoCoordinator);
    }

    @Test
    public void testInitWithNativeWhenParentSurfaceIsVisible() {
        createCoordinatorWithoutNative(true);
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ true, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);

        Assert.assertTrue(mLogoCoordinator.isLogoVisible());
        // When parent surface is shown while native library isn't loaded, calling
        // maybeLoadSearchProviderLogo() will add a pending load task.
        Assert.assertTrue(mLogoCoordinator.getIsLoadPendingForTesting());
        mLogoCoordinator.initWithNative();

        Assert.assertTrue(mLogoCoordinator.isLogoVisible());
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
        verify(mTemplateUrlService).addObserver(mLogoCoordinator);
    }

    @Test
    public void testMaybeLoadSearchProviderLogo() {
        createCoordinator();

        // If parent surface is not shown nor delegate shouldn't be destroyed, logo shouldn't be
        // loaded and delegate isn't destroyed.
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ false, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);
        Assert.assertFalse(mLogoCoordinator.isLogoVisible());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
        verify(mLogoDelegate, times(0)).destroy();

        // If parent surface is not shown and delegate should be destroyed, logo should be
        // loaded and delegate is destroyed.
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ false, /*shouldDestroyDelegate*/ true,
                /*animationEnabled*/ false);
        Assert.assertFalse(mLogoCoordinator.isLogoVisible());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
        verify(mLogoDelegate, times(1)).destroy();
        Assert.assertNull(mLogoCoordinator.getLogoDelegateForTesting());

        // If parent surface is shown, logo should be loaded and delegate shouldn't be
        // destroyed.
        mLogoCoordinator.setLogoDelegateForTesting(mLogoDelegate);
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ true, /*shouldDestroyDelegate*/ false,
                /*animationEnabled*/ false);
        Assert.assertTrue(mLogoCoordinator.isLogoVisible());
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
        verify(mLogoDelegate, times(1)).destroy();
    }

    @Test(expected = AssertionError.class)
    public void testMaybeLoadSearchProviderLogoAssertionError() {
        createCoordinator();
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());

        // If parent surface is shown and delegate should be destroyed, an assertion error
        // should be thrown.
        mLogoCoordinator.maybeLoadSearchProviderLogo(
                /*isParentSurfaceShown*/ true, /*shouldDestroyDelegate*/ true,
                /*animationEnabled*/ false); // should throw an exception
    }

    private void createCoordinator() {
        createCoordinatorWithoutNative(/*isParentSurfaceShown=*/true);
        mLogoCoordinator.initWithNative();
    }

    private void createCoordinatorWithoutNative(boolean isParentSurfaceShown) {
        mLogoCoordinator = new LogoCoordinator(mLogoClickedCallback, mLogoView,
                /*shouldFetchDoodle=*/true, mOnLogoAvailableCallback,
                mOnCachedLogoRevalidatedRunnable, isParentSurfaceShown);
        mLogoCoordinator.setLogoDelegateForTesting(mLogoDelegate);
    }
}
