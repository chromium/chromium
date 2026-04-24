// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoUtils.DoodleSize;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.LoadingView;

/** Unit tests for the {@link LogoContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoContainerViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModel mLogoModel;
    private LogoContainerView mLogoContainerView;
    private LogoMediator mLogoMediator;
    private static final double DELTA = 1e-5;
    private static final String ANIMATED_LOGO_URL =
            "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json";

    @Mock private LogoContainerView mMockLogoView;

    @Mock LogoBridge.Natives mLogoBridgeJniMock;

    @Mock LogoBridge mLogoBridge;

    @Mock ImageFetcher mImageFetcher;

    static class TestObserver implements LoadingView.Observer {
        public final CallbackHelper showLoadingCallback = new CallbackHelper();
        public final CallbackHelper hideLoadingCallback = new CallbackHelper();

        @Override
        public void onShowLoadingUiComplete() {
            showLoadingCallback.notifyCalled();
        }

        @Override
        public void onHideLoadingUiComplete() {
            hideLoadingCallback.notifyCalled();
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mLogoContainerView =
                (LogoContainerView)
                        LayoutInflater.from(mActivity).inflate(R.layout.logo_view_layout, null);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mActivity.setContentView(mLogoContainerView, params);
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mLogoModel, mLogoContainerView, new LogoContainerViewBinder());
        mLogoMediator =
                new LogoMediator(
                        /* logoClickedCallback= */ null,
                        mLogoModel,
                        /* onLogoAvailableCallback= */ null,
                        /* visibilityObserver= */ null,
                        /* defaultGoogleLogoDrawable= */ null);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mLogoModel = null;
        mLogoContainerView = null;
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

        assertEquals(View.VISIBLE, mLogoContainerView.getVisibility());
        assertEquals(0.3, mLogoContainerView.getAlpha(), DELTA);

        // Check container layout params for bottom margin
        ViewGroup.MarginLayoutParams containerParams =
                (ViewGroup.MarginLayoutParams) mLogoContainerView.getLayoutParams();
        assertEquals(20, containerParams.bottomMargin);

        // Check child LogoView layout params for top margin
        LogoView childLogoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        ViewGroup.MarginLayoutParams childParams =
                (ViewGroup.MarginLayoutParams) childLogoView.getLayoutParams();
        assertEquals(10, childParams.topMargin);

        mLogoModel.set(LogoProperties.VISIBILITY, false);
        assertEquals(View.GONE, mLogoContainerView.getVisibility());
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
        assertNull(mLogoContainerView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.LOGO, logo);
        assertNotNull(mLogoContainerView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoContainerView.getFadeAnimationForTesting());
        Logo newLogo =
                new Logo(
                        Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888),
                        "https://www.google.com",
                        null,
                        null);
        mLogoModel.set(LogoProperties.LOGO, newLogo);
        assertNotNull(mLogoContainerView.getFadeAnimationForTesting());
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
        assertNull(mLogoContainerView.getFadeAnimationForTesting());
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
        assertNull(mLogoContainerView.getFadeAnimationForTesting());
        assertNotEquals(logo.image, mLogoContainerView.getNewLogoDrawableBitmapForTesting());
        mLogoModel.set(LogoProperties.LOGO, logo);
        assertNotNull(mLogoContainerView.getFadeAnimationForTesting());
        assertEquals(logo.image, mLogoContainerView.getNewLogoDrawableBitmapForTesting());
    }

    @Test
    @SmallTest
    public void testUpdateLogo_Null_ShowsDefault() {
        Drawable defaultLogo =
                ContextCompat.getDrawable(
                        mLogoContainerView.getContext(), R.drawable.ic_google_logo);
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO_DRAWABLE, defaultLogo);
        mLogoContainerView.setLoadingViewVisibilityForTesting(View.VISIBLE);
        mLogoModel.set(LogoProperties.LOGO, null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(View.GONE, mLogoContainerView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @SmallTest
    public void testUpdateLogo_Null_ClearsLogo() {
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO_DRAWABLE, null);
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null, null);
        mLogoModel.set(LogoProperties.LOGO, logo);
        mLogoModel.set(LogoProperties.LOGO, null);
        LogoView childLogoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        assertNull(childLogoView.getLogoDrawableForTesting());
    }

    @Test
    @SmallTest
    public void testDefaultGoogleLogo() {
        Drawable defaultLogo =
                ContextCompat.getDrawable(
                        mLogoContainerView.getContext(), R.drawable.ic_google_logo);
        assertNotEquals(defaultLogo, mLogoContainerView.getDefaultGoogleLogoDrawableForTesting());
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO_DRAWABLE, defaultLogo);
        assertEquals(defaultLogo, mLogoContainerView.getDefaultGoogleLogoDrawableForTesting());
    }

    @Test
    @SmallTest
    public void testAnimationEnabled() {
        assertEquals(true, mLogoContainerView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, false);
        assertEquals(false, mLogoContainerView.getAnimationEnabledForTesting());
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, true);
        assertEquals(true, mLogoContainerView.getAnimationEnabledForTesting());
    }

    @Test
    @SmallTest
    public void testSetLogoClickHandler() {
        assertNull(mLogoContainerView.getClickHandlerForTesting());
        mLogoMediator.setLogoBridgeForTesting(mLogoBridge);
        mLogoMediator.setImageFetcherForTesting(mImageFetcher);
        mLogoMediator.setAnimatedLogoUrlForTesting(ANIMATED_LOGO_URL);
        mLogoModel.set(LogoProperties.LOGO_CLICK_HANDLER, mLogoMediator::onLogoClicked);
        mLogoContainerView.findViewById(R.id.search_provider_logo).performClick();
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting("NewTabPage.LogoClick", 1));
        verify(mImageFetcher, times(1)).fetchGif(any(), any());
    }

    @Test
    @SmallTest
    public void testShowSearchProviderInitialView() {
        PropertyModel logoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                logoModel, mMockLogoView, new LogoContainerViewBinder());
        logoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView).showSearchProviderInitialView();
        logoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
        verify(mMockLogoView, times(2)).showSearchProviderInitialView();
    }

    @Test
    @SmallTest
    public void testLoadingViewWithAnimatedLogo() {
        mLogoContainerView.setLoadingViewVisibilityForTesting(View.INVISIBLE);
        mLogoModel.set(LogoProperties.ANIMATED_LOGO, new BaseGifImage(new byte[] {}));
        assertEquals(View.GONE, mLogoContainerView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @SmallTest
    public void testLoadingViewWithAnimatedImageDrawable() {
        mLogoContainerView.setLoadingViewVisibilityForTesting(View.INVISIBLE);
        mLogoModel.set(LogoProperties.ANIMATED_LOGO, mock(AnimatedImageDrawable.class));
        assertEquals(View.GONE, mLogoContainerView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @SmallTest
    public void testSetDoodleSize() {
        assertEquals(DoodleSize.TABLET_SPLIT_SCREEN, mLogoContainerView.getDoodleSizeForTesting());
        mLogoModel.set(LogoProperties.DOODLE_SIZE, DoodleSize.REGULAR);
        assertEquals(DoodleSize.REGULAR, mLogoContainerView.getDoodleSizeForTesting());
        mLogoModel.set(LogoProperties.DOODLE_SIZE, DoodleSize.TABLET_SPLIT_SCREEN);
        assertEquals(DoodleSize.TABLET_SPLIT_SCREEN, mLogoContainerView.getDoodleSizeForTesting());
    }

    @Test
    @SmallTest
    public void testShowDefaultGoogleLogo() {
        PropertyModel logoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                logoModel, mMockLogoView, new LogoContainerViewBinder());

        logoModel.set(LogoProperties.SHOW_DEFAULT_GOOGLE_LOGO, true);
        verify(mMockLogoView).maybeShowDefaultLogoDrawable();
    }

    @Test
    @SmallTest
    public void testSetLogoTopMargin() {
        mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, 10);
        LogoView childLogoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        MarginLayoutParams params = (MarginLayoutParams) childLogoView.getLayoutParams();
        assertEquals(10, params.topMargin);
    }

    @Test
    @SmallTest
    public void testSetLogoBottomMargin() {
        mLogoModel.set(LogoProperties.LOGO_BOTTOM_MARGIN, 20);
        MarginLayoutParams params = (MarginLayoutParams) mLogoContainerView.getLayoutParams();
        assertEquals(20, params.bottomMargin);
    }

    @Test
    @SmallTest
    public void testSetLogoHeight() {
        mLogoModel.set(LogoProperties.LOGO_HEIGHT, 50);
        LogoView childLogoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        MarginLayoutParams params = (MarginLayoutParams) childLogoView.getLayoutParams();
        assertEquals(50, params.height);
    }

    @Test
    @SmallTest
    public void testShowLoadingView() {
        mLogoContainerView.setLoadingViewVisibilityForTesting(View.GONE);
        mLogoModel.set(LogoProperties.SHOW_LOADING_VIEW, true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(View.VISIBLE, mLogoContainerView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @SmallTest
    public void testSetLogoAvailableCallback() {
        final boolean[] callbackCalled = new boolean[1];
        Callback<Logo> callback = (logo) -> callbackCalled[0] = true;
        mLogoModel.set(LogoProperties.LOGO_AVAILABLE_CALLBACK, callback);
        assertFalse(callbackCalled[0]);

        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888), null, null, null);
        mLogoModel.set(LogoProperties.LOGO, logo);
        mLogoContainerView.endAnimationsForTesting();
        assertTrue(callbackCalled[0]);
    }
}
