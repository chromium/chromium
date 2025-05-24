// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.hamcrest.Matchers.equalTo;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.provider.Settings;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;

/** Test for {@link DefaultBrowserPromoMessageController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
public class DefaultBrowserPromoMessageControllerTest {
    @Mock private Tracker mTestTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private Runnable mOnDisplayChangedCallback;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Activity mActivity;
    private ShadowActivity mShadowActivity;
    DefaultBrowserPromoMessageController mMessageController;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(mActivity);
        mMessageController = new DefaultBrowserPromoMessageController(mActivity, mTestTracker);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void testOnPrimaryButtonClicked() {
        PropertyModel messageProperties = mMessageController.buildPropertyModel();

        Assert.assertEquals(
                Integer.valueOf(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY),
                messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get());

        Intent intent = mShadowActivity.getNextStartedActivity();
        Assert.assertThat(
                intent.getAction(), equalTo(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS));
        verify(mTestTracker, times(1)).notifyEvent("default_browser_promo_messages_used");
    }

    @Test
    public void testOnMessageDismiss() {
        PropertyModel messageProperties = mMessageController.buildPropertyModel();

        messageProperties.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.TIMER);
        verify(mTestTracker, never()).notifyEvent("default_browser_promo_messages_dismissed");

        messageProperties.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.GESTURE);
        verify(mTestTracker, times(1)).notifyEvent("default_browser_promo_messages_dismissed");
    }
}
