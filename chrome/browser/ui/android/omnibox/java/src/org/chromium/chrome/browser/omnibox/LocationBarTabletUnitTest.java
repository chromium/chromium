// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.Gravity;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.ToastManager;

/** Unit tests for LocationBarTablet. */
@RunWith(BaseRobolectricTestRunner.class)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class LocationBarTabletUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private LocationBarTablet mLocationBarTablet;

    @Before
    public void doBeforeEachTest() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        LinearLayout contentView = new LinearLayout(mActivity);
        mLocationBarTablet = new LocationBarTablet(mActivity, null);
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        contentView.addView(mLocationBarTablet, params);
        mLocationBarTablet.onFinishInflate();
        params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mActivity.setContentView(contentView, params);
    }

    @Test
    public void testOnLongClick() {
        longClickAndVerifyToast(R.id.bookmark_button, R.string.menu_bookmark);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testFuseboxStateChange() {
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(LayoutParams.WRAP_CONTENT, layoutParams.height);
        assertEquals(-expansionPx, layoutParams.leftMargin);
        assertEquals(-expansionPx, layoutParams.rightMargin);
        assertEquals(-expansionPx, layoutParams.topMargin);
        assertEquals(Gravity.TOP, layoutParams.gravity);
        assertEquals(expansionPx, mLocationBarTablet.getPaddingLeft());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingRight());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingTop());
        assertEquals(1.0f, mLocationBarTablet.getTranslationZ(), MathUtils.EPSILON);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        layoutParams = (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modern_toolbar_tablet_background_size),
                layoutParams.height);
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(0, layoutParams.rightMargin);
        assertEquals(0, layoutParams.topMargin);
        assertEquals(Gravity.CENTER_VERTICAL, layoutParams.gravity);
        assertEquals(0, mLocationBarTablet.getPaddingLeft());
        assertEquals(0, mLocationBarTablet.getPaddingRight());
        assertEquals(0, mLocationBarTablet.getPaddingTop());
        assertEquals(0.0f, mLocationBarTablet.getTranslationZ(), MathUtils.EPSILON);
    }

    private void longClickAndVerifyToast(int viewId, int stringId) {
        mLocationBarTablet.onLongClick(mLocationBarTablet.findViewById(viewId));
        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(stringId), R.id.toast_text));
        ToastManager.resetForTesting();
    }
}
