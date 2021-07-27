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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link VoiceToolbarButtonController}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {VoiceToolbarButtonControllerUnitTest.ShadowChromeFeatureList.class})
public final class VoiceToolbarButtonControllerUnitTest {
    // TODO(crbug.com/1199025): Remove this shadow.
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        private static final Map<String, String> sParamValues = new HashMap<>();

        @Implementation
        public static String getFieldTrialParamByFeature(String feature, String paramKey) {
            Assert.assertTrue(ChromeFeatureList.isEnabled(feature));
            return sParamValues.getOrDefault(paramKey, "");
        }

        public static void reset() {
            sParamValues.clear();
        }
    }

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
    @Mock
    private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private VoiceToolbarButtonController mVoiceToolbarButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowChromeFeatureList.reset();

        mConfiguration.screenWidthDp = VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP;
        doReturn(mConfiguration).when(mResources).getConfiguration();
        doReturn(mResources).when(mContext).getResources();

        doReturn(true).when(mVoiceSearchDelegate).isVoiceSearchEnabled();

        doReturn(false).when(mTab).isIncognito();

        doReturn("https").when(mUrl).getScheme();
        doReturn(mUrl).when(mTab).getUrl();

        doReturn(mContext).when(mTab).getContext();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        // clang-format off
        mVoiceToolbarButtonController = new VoiceToolbarButtonController(mContext, mDrawable,
                () -> mTab, () -> mTracker, mActivityLifecycleDispatcher, mModalDialogManager,
                mVoiceSearchDelegate);
        // clang-format on

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
            ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    public void
    onConfigurationChanged_screenWidthChanged() {
        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());

        // Screen width shrinks below the threshold (e.g. screen rotated).
        mConfiguration.screenWidthDp = VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP - 1;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertFalse(mVoiceToolbarButtonController.get(mTab).canShow());

        // Make sure the opposite works as well.
        mConfiguration.screenWidthDp = VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    public void
    testIPHCommandHelper() {
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

    @Test
    @DisableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void
    testIPHEvent() {
        doReturn(true).when(mTracker).shouldTriggerHelpUI(
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE);

        View view = Mockito.mock(View.class);
        mVoiceToolbarButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_OPENED);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void isToolbarMicEnabled_adaptiveButtons_nonVoice() {
        ShadowChromeFeatureList.sParamValues.put("mode", AdaptiveToolbarFeatures.ALWAYS_NEW_TAB);
        assertFalse(VoiceToolbarButtonController.isToolbarMicEnabled());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void isToolbarMicEnabled_adaptiveButtons_voice() {
        ShadowChromeFeatureList.sParamValues.put("mode", AdaptiveToolbarFeatures.ALWAYS_VOICE);
        assertTrue(VoiceToolbarButtonController.isToolbarMicEnabled());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    // clang-format off
    public void isToolbarMicEnabled_toolbarMic() {
        // clang-format on
        assertTrue(VoiceToolbarButtonController.isToolbarMicEnabled());
    }
}
