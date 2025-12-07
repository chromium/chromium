// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
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
        FrameLayout contentView = new FrameLayout(mActivity);
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

    private void longClickAndVerifyToast(int viewId, int stringId) {
        mLocationBarTablet.onLongClick(mLocationBarTablet.findViewById(viewId));
        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(stringId), R.id.toast_text));
        ToastManager.resetForTesting();
    }
}
