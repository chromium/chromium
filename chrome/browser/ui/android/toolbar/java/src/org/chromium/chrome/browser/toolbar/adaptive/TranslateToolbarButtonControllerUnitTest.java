// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TranslateToolbarButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TranslateToolbarButtonControllerUnitTest {
    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Mock private WebContents mWebContents;
    @Mock private Tab mTab;
    @Mock private Drawable mDrawable;
    @Mock private Tracker mTracker;
    @Mock TranslateBridge.Natives mMockTranslateBridge;
    @Mock private NativePage mNativePage;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mMockTranslateBridge);
        mActionTester = new UserActionTester();

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
    }

    @After
    public void tearDown() throws Exception {
        mActionTester.tearDown();
    }

    @Test
    public void testButtonData() {
        TranslateToolbarButtonController translateToolbarButtonController =
                new TranslateToolbarButtonController(
                        () -> mTab, mDrawable, "Translate button description", () -> mTracker);
        ButtonData buttonData = translateToolbarButtonController.get(mTab);

        Assert.assertTrue(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
    }

    @Test
    public void testOnClick() {
        TranslateToolbarButtonController translateToolbarButtonController =
                new TranslateToolbarButtonController(
                        () -> mTab, mDrawable, "Translate button description", () -> mTracker);
        translateToolbarButtonController.onClick(null);

        Assert.assertEquals(1, mActionTester.getActionCount("MobileTopToolbarTranslateButton"));
        verify(mTracker)
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_TRANSLATE_OPENED);
        verify(mMockTranslateBridge).manualTranslateWhenReady(mWebContents);
    }

    @Test
    public void testShouldNotShowUpOnNonHttpUrls() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_ABOUT);
        TranslateToolbarButtonController translateToolbarButtonController =
                new TranslateToolbarButtonController(
                        () -> mTab, mDrawable, "Translate button description", () -> mTracker);
        ButtonData buttonData = translateToolbarButtonController.get(mTab);

        Assert.assertFalse(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
    }

    @Test
    public void testShouldNotShowUpPdfUrls() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.HTTP_URL);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.isPdf()).thenReturn(true);
        TranslateToolbarButtonController translateToolbarButtonController =
                new TranslateToolbarButtonController(
                        () -> mTab, mDrawable, "Translate button description", () -> mTracker);
        ButtonData buttonData = translateToolbarButtonController.get(mTab);

        Assert.assertFalse(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
    }
}
