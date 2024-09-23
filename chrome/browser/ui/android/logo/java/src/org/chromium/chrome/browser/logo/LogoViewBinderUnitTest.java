// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.SmallTest;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoUtils.LogoSizeForLogoPolish;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.LoadingView;

// TODO(crbug.com/40881870): For the LogoViewTest and LogoViewBinderUnitTest, that's the nice thing
//  about only have 1 test file, where all test cases go into the single test file.

/** Unit tests for the {@link LogoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoViewBinderUnitTest {
    private Activity mActivity;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModel mLogoModel;
    private LogoView mLogoView;
    private LogoMediator mLogoMediator;
    private static final double DELTA = 1e-5;
    private static final String ANIMATED_LOGO_URL =
            "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private LogoView mMockLogoView;

    @Mock LogoBridge.Natives mLogoBridgeJniMock;

    @Mock LogoBridge mLogoBridge;

    @Mock ImageFetcher mImageFetcher;

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
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridgeJniMock);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mLogoView = new LogoView(mActivity, null);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mActivity.setContentView(mLogoView, params);
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());
        mLogoMediator =
                new LogoMediator(
                        /* context= */ null,
                        /* logoClickedCallback= */ null,
                        mLogoModel,
                        /* shouldFetchDoodle= */ true,
                        /* onLogoAvailableCallback= */ null,
                        /* visibilityObserver= */ null,
                        /* defaultGoogleLogo= */ null);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mLogoModel = null;
        mLogoView = null;
        mActivity = null;
        mLogoMediator = null;
    }

    @Test
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
    @SmallTest
    public void testEndFadeAnimation() {
        Logo logo =
                new Logo(
                        Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8),
                        null,
                        null,
                        "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json");
        assertNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.LOGO, logo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoView.getFadeAnimationForTesting());
        Logo newLogo =
                new Logo(
                        Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888),
                        "https://www.google.com",
                        null,
                        null);
        mLogoModel.set(LogoProperties.LOGO, newLogo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoView.getFadeAnimationForTesting());
    }

    @Test
    @SmallTest
    public void testUpdateLogo() {
        Logo logo =
                new Logo(
                        Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8),
                        null,
                        null,
                        "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json");
        assertNull(mLogoView.getFadeAnimationForTesting());
        assertNotEquals(logo.image, mLogoView.getNewLogoForTesting());
        mLogoModel.set(LogoProperties.LOGO, logo);
        assertNotNull(mLogoView.getFadeAnimationForTesting());
        assertEquals(logo.image, mLogoView.getNewLogoForTesting());
    }

    @Test
    @SmallTest
    public void testDefaultGoogleLogo() {
        Bitmap defaultLogo =
                BitmapFactory.decodeResource(
                        mLogoView.getContext().getResources(), R.drawable.google_logo);
        assertNotEquals(defaultLogo, mLogoView.getDefaultGoogleLogoForTesting());
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO, defaultLogo);
        assertEquals(defaultLogo, mLogoView.getDefaultGoogleLogoForTesting());
    }

    @Test
    @SmallTest
    public void testAnimationEnabled() {
        assertEquals(true, mLogoView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, false);
        assertEquals(false, mLogoView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, true);
        assertEquals(true, mLogoView.getAnimationEnabledForTesting());
    }

    @Test
    @SmallTest
    public void testSetLogoClickHandler() {
        assertNull(mLogoView.getClickHandlerForTesting());
        mLogoMediator.setLogoBridgeForTesting(mLogoBridge);
        mLogoMediator.setImageFetcherForTesting(mImageFetcher);
        mLogoMediator.setAnimatedLogoUrlForTesting(ANIMATED_LOGO_URL);
        mLogoModel.set(LogoProperties.LOGO_CLICK_HANDLER, mLogoMediator::onLogoClicked);
        mLogoView.onClick(mLogoView);
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting("NewTabPage.LogoClick", 1));
        verify(mImageFetcher, times(1)).fetchGif(any(), any());
    }

    @Test
    @SmallTest
    public void testShowSearchProviderInitialView() {
        PropertyModel LogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(LogoModel, mMockLogoView, new LogoViewBinder());
        LogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView).showSearchProviderInitialView();
        LogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView, times(2)).showSearchProviderInitialView();
    }

    @Test
    @SmallTest
    public void testLoadingViewWithAnimatedLogo() {
        mLogoView.setLoadingViewVisibilityForTesting(View.INVISIBLE);
        mLogoModel.set(LogoProperties.ANIMATED_LOGO, new BaseGifImage(new byte[] {}));
        assertEquals(View.GONE, mLogoView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @SmallTest
    public void testLogoPolishFlagEnabled() {
        assertEquals(false, mLogoView.getIsLogoPolishFlagEnabledForTesting());
        mLogoModel.set(LogoProperties.LOGO_POLISH_FLAG_ENABLED, true);
        assertEquals(true, mLogoView.getIsLogoPolishFlagEnabledForTesting());
        mLogoModel.set(LogoProperties.LOGO_POLISH_FLAG_ENABLED, false);
        assertEquals(false, mLogoView.getIsLogoPolishFlagEnabledForTesting());
    }

    @Test
    @SmallTest
    public void testSetLogoSizeForLogoPolish() {
        assertEquals(LogoSizeForLogoPolish.SMALL, mLogoView.getLogoSizeForLogoPolishForTesting());
        mLogoModel.set(LogoProperties.LOGO_SIZE_FOR_LOGO_POLISH, LogoSizeForLogoPolish.MEDIUM);
        assertEquals(LogoSizeForLogoPolish.MEDIUM, mLogoView.getLogoSizeForLogoPolishForTesting());
        mLogoModel.set(LogoProperties.LOGO_SIZE_FOR_LOGO_POLISH, LogoSizeForLogoPolish.LARGE);
        assertEquals(LogoSizeForLogoPolish.LARGE, mLogoView.getLogoSizeForLogoPolishForTesting());
        mLogoModel.set(LogoProperties.LOGO_SIZE_FOR_LOGO_POLISH, LogoSizeForLogoPolish.SMALL);
        assertEquals(LogoSizeForLogoPolish.SMALL, mLogoView.getLogoSizeForLogoPolishForTesting());
    }
}
