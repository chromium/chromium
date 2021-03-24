// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** Unit tests for {@link VoiceToolbarButtonController}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class VoiceToolbarButtonControllerUnitTest {
    private static final int WIDTH_DELTA = 50;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private Tab mTab;
    @Mock
    private GURL mUrl;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private ModalDialogManager mModalDialogManager;
    @Mock
    private VoiceToolbarButtonController.VoiceSearchDelegate mVoiceSearchDelegate;
    @Mock
    private Drawable mDrawable;

    private Configuration mConfiguration = new Configuration();
    private VoiceToolbarButtonController mVoiceToolbarButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP + WIDTH_DELTA;
        doReturn(mConfiguration).when(mResources).getConfiguration();
        doReturn(mResources).when(mContext).getResources();

        doReturn(true).when(mVoiceSearchDelegate).isVoiceSearchEnabled();

        doReturn(false).when(mTab).isIncognito();

        doReturn("https").when(mUrl).getScheme();
        doReturn(mUrl).when(mTab).getUrl();

        doReturn(mContext).when(mTab).getContext();

        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, false);
        // clang-format off
        mVoiceToolbarButtonController = new VoiceToolbarButtonController(mContext, mDrawable,
                () -> mTab, mActivityLifecycleDispatcher, mModalDialogManager,
                mVoiceSearchDelegate);
        // clang-format on
    }

    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    @Test
    public void onConfigurationChanged_screenWidthChanged() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);
        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());

        // Screen width shrinks below the threshold (e.g. screen rotated).
        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP - WIDTH_DELTA;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertFalse(mVoiceToolbarButtonController.get(mTab).canShow());

        // Make sure the opposite works as well.
        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP + WIDTH_DELTA;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());
    }

    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    @Test
    public void
    testIPHCommandHelper() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);
        assertNull(mVoiceToolbarButtonController.get(/*tab*/ null)
                           .getButtonSpec()
                           .getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();
        assertNotNull(
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
        assertEquals(builder,
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }
}
