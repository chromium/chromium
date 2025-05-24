// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.Mockito.doReturn;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ToolbarUnitTestUtils;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.util.DrawableUtils;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.io.IOException;

/** Render tests for {@link StatusView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class StatusViewRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private StatusView mStatusView;
    private PropertyModel mStatusModel;
    private LocationBarModel mLocationBarModel;
    private Drawable mBackground;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        ChromeAutocompleteSchemeClassifierJni.setInstanceForTesting(
                mChromeAutocompleteSchemeClassifierJni);

        doReturn(true).when(mIncognitoProfile).isOffTheRecord();

        runOnUiThreadBlocking(
                () -> {
                    ViewGroup view = new LinearLayout(sActivity);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.WRAP_CONTENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);

                    sActivity.setContentView(view, params);

                    mStatusView =
                            sActivity
                                    .getLayoutInflater()
                                    .inflate(R.layout.location_status, view, true)
                                    .findViewById(R.id.location_bar_status);
                    mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(view));
                    mLocationBarModel =
                            new LocationBarModel(
                                    mStatusView.getContext(),
                                    NewTabPageDelegate.EMPTY,
                                    url -> url.getSpec(),
                                    ToolbarUnitTestUtils.OFFLINE_STATUS);
                    mLocationBarModel.setTab(null, mProfile);
                    mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
                    PropertyModelChangeProcessor.create(
                            mStatusModel, mStatusView, new StatusViewBinder());

                    // Increases visibility for manual parsing of diffs. Status view matches the
                    // parent height, so this white will stretch vertically.
                    mStatusView.setBackgroundColor(Color.WHITE);

                    int size =
                            mStatusView
                                    .getContext()
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.small_icon_background_size);
                    mBackground =
                            DrawableUtils.getIconBackground(
                                    mStatusView.getContext(), false, size, size);
                });
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(() -> mLocationBarModel.destroy());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewIncognitoWithIcon() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mLocationBarModel.setTab(null, mIncognitoProfile);
                    mStatusView.setIncognitoBadgeVisibility(true);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_search, 0));
                });
        mRenderTestRule.render(mStatusView, "status_view_incognito_with_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewIncognitoNoIcon() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mLocationBarModel.setTab(null, mIncognitoProfile);
                    mStatusView.setIncognitoBadgeVisibility(true);
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
                });
        mRenderTestRule.render(mStatusView, "status_view_incognito_no_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithIcon() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
                    mStatusModel.set(StatusProperties.STATUS_VIEW_BACKGROUND, mBackground);
                    mStatusModel.set(
                            StatusProperties.STATUS_VIEW_TOOLTIP_TEXT,
                            R.string.accessibility_menu_info);
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_search, 0));
                });
        mRenderTestRule.render(mStatusView, "status_view_with_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithLocationPermissionIcon() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    Drawable locationIcon =
                            ContentSettingsResources.getIconForOmnibox(
                                    mStatusView.getContext(),
                                    ContentSettingsType.GEOLOCATION,
                                    ContentSettingValues.ALLOW,
                                    false);
                    PermissionIconResource statusIcon =
                            new PermissionIconResource(locationIcon, false);
                    statusIcon.setTransitionType(StatusView.IconTransitionType.ROTATE);
                    mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
                    mStatusModel.set(StatusProperties.STATUS_VIEW_BACKGROUND, mBackground);
                    mStatusModel.set(
                            StatusProperties.STATUS_VIEW_TOOLTIP_TEXT,
                            R.string.accessibility_menu_info);
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
                });
        mRenderTestRule.render(mStatusView, "status_view_with_location_permission_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithStoreIcon() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    Drawable storeIconDrawable =
                            ResourcesCompat.getDrawable(
                                    sActivity.getResources(),
                                    R.drawable.ic_storefront_blue,
                                    sActivity.getTheme());
                    StatusIconResource statusIcon =
                            new PermissionIconResource(storeIconDrawable, false);
                    statusIcon.setTransitionType(StatusView.IconTransitionType.ROTATE);
                    mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
                    mStatusModel.set(StatusProperties.STATUS_VIEW_BACKGROUND, mBackground);
                    mStatusModel.set(
                            StatusProperties.STATUS_VIEW_TOOLTIP_TEXT,
                            R.string.accessibility_menu_info);
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
                });
        mRenderTestRule.render(mStatusView, "status_view_with_store_icon");
    }
}
