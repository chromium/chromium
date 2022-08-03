// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for the {@link LogoLoadHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoLoadHelperUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    Profile mMockProfile1;

    @Mock
    Profile mMockProfile2;

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

    private Context mContext;
    private LogoLoadHelper mLogoLoadHelper;
    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mProfileSupplier = new ObservableSupplierImpl<>();
        mContext = ApplicationProvider.getApplicationContext();
        mLogoLoadHelper = new LogoLoadHelper(mProfileSupplier, mLogoClickedCallback, mLogoView);
        mLogoLoadHelper.setLogoDelegateForTesting(mLogoDelegate);

        doReturn(false).when(mMockProfile1).isOffTheRecord();
        doReturn(true).when(mMockProfile2).isOffTheRecord();

        mJniMocker.mock(TemplateUrlServiceFactoryJni.TEST_HOOKS, mTemplateUrlServiceFactory);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mTemplateUrlService).doesDefaultSearchEngineHaveLogo();

        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);

        CachedFeatureFlags.setForTesting(ChromeFeatureList.START_SURFACE_ANDROID, true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(true));
        mProfileSupplier.set(mMockProfile1);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @Test
    public void testDSEChangedOnRegularProfileAndGoogleIsDSE() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        StartSurfaceConfiguration.IS_DOODLE_SUPPORTED.setForTesting(true);

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        Assert.assertNotNull(LogoView.getDefaultGoogleLogo(mContext));
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedOnRegularProfileAndGoogleIsNotDSE() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        Assert.assertNull(LogoView.getDefaultGoogleLogo(mContext));
        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedOnRegularProfileAndDoesNotHaveLogo() {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedWithIncognitoProfile() {
        mProfileSupplier.set(mMockProfile2);

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testLoadLogoOnStartSurfaceHomepageWhenLogoNotLoaded() {
        mLogoLoadHelper.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.maybeLoadSearchProviderLogoOnHomepage(
                /*isStartSurfaceShown*/ true, /*isStartSurfaceHidden*/ false);

        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
    }

    @Test
    public void testLoadLogoOnStartSurfaceHomepageWhenLogoHasLoaded() {
        mLogoLoadHelper.setHasLogoLoadedForCurrentSearchEngineForTesting(true);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.maybeLoadSearchProviderLogoOnHomepage(
                /*isStartSurfaceShown*/ true, /*isStartSurfaceHidden*/ false);

        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }
}
