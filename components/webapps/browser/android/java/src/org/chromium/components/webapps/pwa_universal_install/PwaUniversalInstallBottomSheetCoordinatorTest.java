// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.AppType;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.WebappsFeatureMap;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.url.GURL;

/** Instrumentation tests for PWA Universal Install bottom sheet. */
@RunWith(BaseRobolectricTestRunner.class)
public class PwaUniversalInstallBottomSheetCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    Activity mActivity;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Before
    public void setUp() {

        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetchingForTesting = true;
    }

    @After
    public void tearDown() {
        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetchingForTesting = false;
    }

    private void onInstallCalled() {}

    private void onAddShortcutCalled() {}

    private void onOpenAppCalled() {}

    @Test
    @MediumTest
    public void testShowing() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();

        // Setup the coordinator with a mocked WebContents object.
        MockWebContents webContents = mock(MockWebContents.class);
        GURL url = new GURL("http://www.example.com");
        doReturn(url).when(webContents).getLastCommittedUrl();

        PwaUniversalInstallBottomSheetCoordinator coordinator =
                new PwaUniversalInstallBottomSheetCoordinator(
                        activity,
                        webContents,
                        this::onInstallCalled,
                        this::onAddShortcutCalled,
                        this::onOpenAppCalled,
                        /* webAppAlreadyInstalled= */ false,
                        mBottomSheetControllerMock,
                        /* arrowId= */ 0,
                        /* installOverlayId= */ 0,
                        /* shortcutOverlayId= */ 0);

        View view = coordinator.getBottomSheetViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Install and create shortcut",
                            ((TextView) view.findViewById(R.id.title)).getText());
                    Assert.assertEquals(
                            "Install",
                            ((TextView) view.findViewById(R.id.option_text_install)).getText());
                    Assert.assertEquals(
                            "Checking if app can be installed…",
                            ((TextView) view.findViewById(R.id.option_text_install_explanation))
                                    .getText());
                    Assert.assertEquals(
                            "Create shortcut",
                            ((TextView) view.findViewById(R.id.option_text_shortcut)).getText());
                    Assert.assertEquals(
                            "Shortcuts open in Chrome",
                            ((TextView) view.findViewById(R.id.option_text_shortcut_explanation))
                                    .getText());

                    Assert.assertTrue(
                            view.findViewById(R.id.spinny_install).getVisibility() == View.VISIBLE);
                    Assert.assertTrue(
                            view.findViewById(R.id.spinny_shortcut).getVisibility()
                                    == View.VISIBLE);
                    Assert.assertTrue(
                            view.findViewById(R.id.app_icon_install).getVisibility() == View.GONE);
                    Assert.assertTrue(
                            view.findViewById(R.id.app_icon_shortcut).getVisibility() == View.GONE);
                    Assert.assertTrue(
                            view.findViewById(R.id.install_icon_overlay).getVisibility()
                                    == View.GONE);
                    Assert.assertTrue(
                            view.findViewById(R.id.shortcut_icon_overlay).getVisibility()
                                    == View.GONE);
                    Assert.assertTrue(
                            view.findViewById(R.id.arrow_install).getVisibility() == View.VISIBLE);
                    Assert.assertTrue(
                            view.findViewById(R.id.arrow_shortcut).getVisibility() == View.VISIBLE);
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({WebappsFeatureMap.ALWAYS_SHOW_INSTALL_DISAMBIGUATION_DIALOG})
    public void testOnAppDataFetched_alwaysShowDisambiguationFlagEnabled() {
        doReturn(true).when(mBottomSheetControllerMock).requestShowContent(any(), anyBoolean());
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        MockWebContents webContents = mock(MockWebContents.class);
        GURL url = new GURL("http://www.example.com/");
        doReturn(url).when(webContents).getLastCommittedUrl();

        boolean[] installCalled = new boolean[1];
        PwaUniversalInstallBottomSheetCoordinator coordinator =
                new PwaUniversalInstallBottomSheetCoordinator(
                        activity,
                        webContents,
                        () -> installCalled[0] = true,
                        this::onAddShortcutCalled,
                        this::onOpenAppCalled,
                        /* webAppAlreadyInstalled= */ false,
                        mBottomSheetControllerMock,
                        /* arrowId= */ 0,
                        /* installOverlayId= */ 0,
                        /* shortcutOverlayId= */ 0);

        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.onAppDataFetched(AppType.WEBAPK, icon, /* adaptive= */ false);

        // When flag is enabled, auto-redirect to install callback should be suppressed
        // and the bottom sheet content requested to show.
        Assert.assertFalse(installCalled[0]);
        org.mockito.Mockito.verify(mBottomSheetControllerMock)
                .requestShowContent(any(), org.mockito.ArgumentMatchers.eq(true));
    }

    @Test
    @MediumTest
    @DisableFeatures({WebappsFeatureMap.ALWAYS_SHOW_INSTALL_DISAMBIGUATION_DIALOG})
    public void testOnAppDataFetched_flagDisabled_autoRedirectsOnRoot() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        MockWebContents webContents = mock(MockWebContents.class);
        GURL url = new GURL("http://www.example.com/");
        doReturn(url).when(webContents).getLastCommittedUrl();

        boolean[] installCalled = new boolean[1];
        PwaUniversalInstallBottomSheetCoordinator coordinator =
                new PwaUniversalInstallBottomSheetCoordinator(
                        activity,
                        webContents,
                        () -> installCalled[0] = true,
                        this::onAddShortcutCalled,
                        this::onOpenAppCalled,
                        /* webAppAlreadyInstalled= */ false,
                        mBottomSheetControllerMock,
                        /* arrowId= */ 0,
                        /* installOverlayId= */ 0,
                        /* shortcutOverlayId= */ 0);

        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.onAppDataFetched(AppType.WEBAPK, icon, /* adaptive= */ false);

        // When flag is disabled, root URL triggers auto-redirect to install callback.
        Assert.assertTrue(installCalled[0]);
        org.mockito.Mockito.verify(mBottomSheetControllerMock, org.mockito.Mockito.never())
                .requestShowContent(any(), anyBoolean());
    }
}
