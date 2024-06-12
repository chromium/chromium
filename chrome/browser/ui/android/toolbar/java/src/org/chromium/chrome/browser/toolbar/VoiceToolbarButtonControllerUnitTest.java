// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** Unit tests for {@link VoiceToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocks GURL
public final class VoiceToolbarButtonControllerUnitTest {
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Tab mTab;
    @Mock private GURL mUrl;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private VoiceToolbarButtonController.VoiceSearchDelegate mVoiceSearchDelegate;
    @Mock private Drawable mDrawable;
    @Mock private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private VoiceToolbarButtonController mVoiceToolbarButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mConfiguration.screenWidthDp = AdaptiveToolbarFeatures.DEFAULT_MIN_WIDTH_DP;
        doReturn(mConfiguration).when(mResources).getConfiguration();
        doReturn(mResources).when(mContext).getResources();

        doReturn(true).when(mVoiceSearchDelegate).isVoiceSearchEnabled();

        doReturn(false).when(mTab).isIncognito();

        doReturn("https").when(mUrl).getScheme();
        doReturn(mUrl).when(mTab).getUrl();

        doReturn(mContext).when(mTab).getContext();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        mVoiceToolbarButtonController =
                new VoiceToolbarButtonController(
                        mContext,
                        mDrawable,
                        () -> mTab,
                        () -> mTracker,
                        mModalDialogManager,
                        mVoiceSearchDelegate);

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testIPHCommandHelper() {
        assertNull(
                mVoiceToolbarButtonController
                        .get(/* tab= */ null)
                        .getButtonSpec()
                        .getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();

        assertNotNull(
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
        assertEquals(
                builder,
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testIPHEvent() {
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(
                        FeatureConstants
                                .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE);

        View view = Mockito.mock(View.class);
        mVoiceToolbarButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_OPENED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void isToolbarMicEnabled_toolbarMic() {
        assertTrue(VoiceToolbarButtonController.isToolbarMicEnabled());
    }
}
