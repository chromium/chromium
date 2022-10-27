// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.LoadingView;

/** Unit tests for the {@link LogoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoViewBinderUnitTest {
    private Activity mActivity;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModel mLogoModel;
    private LogoView mLogoView;
    private LogoDelegateImpl mLogoDelegate;
    private static final double DELTA = 1e-5;

    @Rule
    public final JniMocker mJniMocker = new JniMocker();

    @Mock
    LogoBridge.Natives mLogoBridge;

    @Mock
    Profile.Natives mProfileJniMock;

    @Mock
    private Profile mProfile;

    @Mock
    private LogoView mMockLogoView;

    static class TestObserver implements LoadingView.Observer {
        public final CallbackHelper showLoadingCallback = new CallbackHelper();
        public final CallbackHelper hideLoadingCallback = new CallbackHelper();

        @Override
        public void onShowLoadingUIComplete() {
            showLoadingCallback.notifyCalled();
        }

        @Override
        public void onHideLoadingUIComplete() {
            hideLoadingCallback.notifyCalled();
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mLogoView = new LogoView(mActivity, null);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mActivity.setContentView(mLogoView, params);
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);
        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        mLogoDelegate = new LogoDelegateImpl(null, mLogoView, mProfile);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mLogoModel = null;
        mLogoView = null;
        mActivity = null;
        mLogoDelegate = null;
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetShowAndHideLogoWithMetaData() {
        assertFalse(mLogoModel.get(LogoProperties.VISIBILITY));
        mLogoModel.set(LogoProperties.ALPHA, (float) 0.3);
        mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, 10);
        mLogoModel.set(LogoProperties.LOGO_BOTTOM_MARGIN, 20);
        mLogoModel.set(LogoProperties.VISIBILITY, true);

        assertEquals(View.VISIBLE, mLogoView.getVisibility());
        assertEquals(0.3, mLogoView.getAlpha(), DELTA);
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) mLogoView.getLayoutParams();
        assertEquals(10, marginLayoutParams.topMargin);
        assertEquals(20, marginLayoutParams.bottomMargin);

        mLogoModel.set(LogoProperties.VISIBILITY, false);
        assertEquals(View.GONE, mLogoView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testEndFadeAnimation() {
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null,
                "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json");
        assertNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.UPDATED_LOGO, logo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoView.getFadeAnimationForTesting());
        Logo newLogo = new Logo(Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888),
                "https://www.google.com", null, null);
        mLogoModel.set(LogoProperties.UPDATED_LOGO, newLogo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoView.getFadeAnimationForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDestroy() {
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null,
                "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json");
        mLogoModel.set(LogoProperties.UPDATED_LOGO, logo);
        mLogoView.addLoadingViewObserverForTesting(new TestObserver());
        assertEquals(false, mLogoView.checkLoadingViewObserverEmptyForTesting());
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.DESTROY, true);
        assertNull(mLogoView.getFadeAnimationForTesting());
        assertEquals(true, mLogoView.checkLoadingViewObserverEmptyForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testUpdateLogo() {
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null,
                "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json");
        assertNull(mLogoView.getFadeAnimationForTesting());
        assertNotEquals(logo.image, mLogoView.getNewLogoForTesting());
        mLogoModel.set(LogoProperties.UPDATED_LOGO, logo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        assertEquals(logo.image, mLogoView.getNewLogoForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDefaultGoogleLogo() {
        Bitmap defaultLogo = BitmapFactory.decodeResource(
                mLogoView.getContext().getResources(), R.drawable.google_logo);
        assertNotEquals(defaultLogo, mLogoView.getDefaultGoogleLogoForTesting());
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO, defaultLogo);
        assertEquals(defaultLogo, mLogoView.getDefaultGoogleLogoForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testAnimationEnabled() {
        assertEquals(true, mLogoView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, false);
        assertEquals(false, mLogoView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, true);
        assertEquals(true, mLogoView.getAnimationEnabledForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLogoDelegate() {
        assertNull(mLogoView.getDelegateForTesting());
        mLogoModel.set(LogoProperties.LOGO_DELEGATE, mLogoDelegate);
        assertEquals(mLogoDelegate, mLogoView.getDelegateForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testShowSearchProviderInitialView() {
        PropertyModel LogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(LogoModel, mMockLogoView, new LogoViewBinder());
        LogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView).showSearchProviderInitialView();
        LogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView, times(2)).showSearchProviderInitialView();
    }
}
