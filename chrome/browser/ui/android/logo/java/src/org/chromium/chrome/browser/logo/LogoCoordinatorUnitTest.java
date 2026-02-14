// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.content_public.browser.LoadUrlParams;

/** Unit tests for the {@link LogoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LogoView mLogoView;
    @Mock private Callback<LoadUrlParams> mLogoClickedCallback;
    @Mock private Callback<Logo> mOnLogoAvailableCallback;
    @Mock private LogoCoordinator.VisibilityObserver mVisibilityObserver;
    @Mock private LogoMediator mLogoMediator;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Captor
    private ArgumentCaptor<NtpCustomizationConfigManager.HomepageStateListener>
            mHomepageStateListenerCaptor;

    private Context mContext;
    private LogoCoordinator mLogoCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testMaybeInitHomepageStateListener_featuresDisabled() {
        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager, never()).addListener(any(), any(), anyBoolean());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testMaybeInitHomepageStateListener_disabledByPolicy() {
        NtpCustomizationPolicyManager policyManager = mock(NtpCustomizationPolicyManager.class);
        NtpCustomizationPolicyManager.setInstanceForTesting(policyManager);
        when(policyManager.isNtpCustomBackgroundEnabled()).thenReturn(false);

        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager, never()).addListener(any(), any(), anyBoolean());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testMaybeInitHomepageStateListener_featuresEnabled() {
        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testHomepageStateListener_logoNotShown() {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo = mock(BackgroundImageInfo.class);

        when(mLogoMediator.isDefaultGoogleLogoShown()).thenReturn(false);
        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));

        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundImageChanged(
                        bitmap,
                        backgroundImageInfo,
                        false,
                        NtpBackgroundType.DEFAULT,
                        NtpBackgroundType.IMAGE_FROM_DISK);

        verify(mLogoMediator, never()).updateDefaultGoogleLogo(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testHomepageStateListener_onBackgroundImageChanged() {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo = mock(BackgroundImageInfo.class);

        when(mLogoMediator.isDefaultGoogleLogoShown()).thenReturn(true);
        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));

        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundImageChanged(
                        bitmap,
                        backgroundImageInfo,
                        false,
                        NtpBackgroundType.DEFAULT,
                        NtpBackgroundType.IMAGE_FROM_DISK);

        verify(mLogoMediator).updateDefaultGoogleLogo(any(Drawable.class));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testHomepageStateListener_onBackgroundColorChanged() {
        @ColorInt int backgroundColor = Color.RED;
        @ColorInt int primaryColor = Color.BLUE;
        NtpThemeColorFromHexInfo colorFromHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        when(mLogoMediator.isDefaultGoogleLogoShown()).thenReturn(true);

        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));

        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundColorChanged(
                        colorFromHexInfo,
                        backgroundColor,
                        false,
                        NtpBackgroundType.DEFAULT,
                        NtpBackgroundType.COLOR_FROM_HEX);

        verify(mLogoMediator).updateDefaultGoogleLogo(any(Drawable.class));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testDestroyRemovesListener() {
        mLogoCoordinator = createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));
        mLogoCoordinator.destroy();
        verify(mNtpCustomizationConfigManager)
                .removeListener(mHomepageStateListenerCaptor.getValue());
    }

    private LogoCoordinator createLogoCoordinator() {
        LogoCoordinator coordinator =
                new LogoCoordinator(
                        mContext,
                        mLogoClickedCallback,
                        mLogoView,
                        mOnLogoAvailableCallback,
                        mVisibilityObserver);
        coordinator.setMediatorForTesting(mLogoMediator);
        return coordinator;
    }
}
