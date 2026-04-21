// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.view.ViewStub;

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
import org.chromium.chrome.browser.logo.LogoUtils.DoodleSize;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.function.Supplier;

/** Unit tests for the {@link LogoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LegacyLogoView mLogoView;
    @Mock private ViewGroup mParentView;
    @Mock private Callback<LoadUrlParams> mLogoClickedCallback;
    @Mock private Callback<Logo> mOnLogoAvailableCallback;
    @Mock private LogoCoordinator.VisibilityObserver mVisibilityObserver;
    @Mock private LogoMediator mLogoMediator;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private Supplier<Boolean> mIsInMultiWindowModeSupplier;

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
        when(mParentView.findViewById(R.id.search_provider_logo)).thenReturn(mLogoView);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(false);
        ViewStub mockStub = mock(ViewStub.class);
        when(mParentView.findViewById(R.id.logo_view_stub)).thenReturn(mockStub);
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
    public void testHomepageStateListener_GoogleLogoNotShown() {
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

        // Test case that another image is selected.
        clearInvocations(mLogoMediator);
        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundImageChanged(
                        Bitmap.createBitmap(20, 20, Bitmap.Config.ARGB_8888),
                        backgroundImageInfo,
                        false,
                        NtpBackgroundType.IMAGE_FROM_DISK,
                        NtpBackgroundType.THEME_COLLECTION);

        verify(mLogoMediator, never()).updateDefaultGoogleLogo(any(Drawable.class));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testHomepageStateListener_onBackgroundColorChanged() {
        when(mLogoMediator.isDefaultGoogleLogoShown()).thenReturn(true);
        @NtpThemeColorId int colorInfoId = NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, colorInfo);

        createLogoCoordinator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(true));

        // Test case that a new color is selected.
        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundColorChanged(
                        colorInfo,
                        backgroundColor,
                        false,
                        NtpBackgroundType.CHROME_COLOR,
                        NtpBackgroundType.DEFAULT);

        verify(mLogoMediator).updateDefaultGoogleLogo(any(Drawable.class));

        // Test case that the newly selected color matches the old logo color.
        clearInvocations(mLogoMediator);
        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundColorChanged(
                        colorInfo,
                        backgroundColor,
                        false,
                        NtpBackgroundType.CHROME_COLOR,
                        NtpBackgroundType.CHROME_COLOR);

        verify(mLogoMediator, never()).updateDefaultGoogleLogo(any(Drawable.class));

        colorInfoId = NtpThemeColorId.NTP_COLORS_VIOLET;
        colorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        backgroundColor = NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, colorInfo);

        // Test case that the newly selected color doesn't match the old logo color.
        clearInvocations(mLogoMediator);
        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundColorChanged(
                        colorInfo,
                        backgroundColor,
                        false,
                        NtpBackgroundType.CHROME_COLOR,
                        NtpBackgroundType.CHROME_COLOR);

        verify(mLogoMediator).updateDefaultGoogleLogo(any(Drawable.class));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testHomepageStateListener_onBackgroundReset() {
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

        // When oldType is not DEFAULT.
        clearInvocations(mLogoMediator);
        mHomepageStateListenerCaptor
                .getValue()
                .onBackgroundReset(NtpBackgroundType.IMAGE_FROM_DISK);
        verify(mLogoMediator).updateDefaultGoogleLogo(any(Drawable.class));

        // When oldType is DEFAULT.
        clearInvocations(mLogoMediator);
        mHomepageStateListenerCaptor.getValue().onBackgroundReset(NtpBackgroundType.DEFAULT);
        verify(mLogoMediator, never()).updateDefaultGoogleLogo(any(Drawable.class));
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

    @Test
    public void testUpdateDoodleOnTablet_setDoodleSize() {
        mLogoCoordinator = createLogoCoordinator();
        verify(mLogoView).setDoodleSize(LogoUtils.DoodleSize.REGULAR);

        // Tablet transitions to multi-window mode.
        verifyDoodleSize(
                /* isInMultiWindowMode= */ true,
                /* showingNonStandardGoogleLogo= */ false,
                DoodleSize.TABLET_SPLIT_SCREEN);

        // Tablet transitions back to regular mode.
        verifyDoodleSize(
                /* isInMultiWindowMode= */ false,
                /* showingNonStandardGoogleLogo= */ false,
                DoodleSize.REGULAR);

        // Tablet transitions to multi-window mode.
        verifyDoodleSize(
                /* isInMultiWindowMode= */ true,
                /* showingNonStandardGoogleLogo= */ true,
                DoodleSize.TABLET_SPLIT_SCREEN);

        // Tablet transitions back to regular mode.
        verifyDoodleSize(
                /* isInMultiWindowMode= */ false,
                /* showingNonStandardGoogleLogo= */ true,
                DoodleSize.REGULAR);
    }

    @Test
    public void testUpdateDoodleOnTablet_setLayoutParams() {
        mLogoCoordinator = createLogoCoordinator();
        verify(mLogoView).setDoodleSize(LogoUtils.DoodleSize.REGULAR);

        // Tablet transitions to multi-window mode.
        clearInvocations(mLogoView);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(true);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ true);
        verify(mLogoView).setLogoHeight(anyInt());
        verify(mLogoView).setLogoTopMargin(anyInt());
    }

    @Test
    public void testUpdateDoodleOnTablet_sameMode() {
        mLogoCoordinator = createLogoCoordinator();
        verify(mLogoView).setDoodleSize(LogoUtils.DoodleSize.REGULAR);

        // Tablet mode doesn't change.
        clearInvocations(mLogoView);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        verify(mLogoView, never()).setDoodleSize(LogoUtils.DoodleSize.REGULAR);

        // Tablet transitions to multi-window mode.
        clearInvocations(mLogoView);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(true);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        verify(mLogoView).setDoodleSize(LogoUtils.DoodleSize.TABLET_SPLIT_SCREEN);

        // Tablet mode doesn't change.
        clearInvocations(mLogoView);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        verify(mLogoView, never()).setDoodleSize(LogoUtils.DoodleSize.TABLET_SPLIT_SCREEN);
    }

    private LogoCoordinator createLogoCoordinator() {
        LogoCoordinator coordinator =
                new LogoCoordinator(
                        mContext,
                        mLogoClickedCallback,
                        mParentView,
                        mOnLogoAvailableCallback,
                        mVisibilityObserver,
                        mIsInMultiWindowModeSupplier);
        coordinator.setMediatorForTesting(mLogoMediator);
        return coordinator;
    }

    private void verifyDoodleSize(
            boolean isInMultiWindowMode,
            boolean showingNonStandardGoogleLogo,
            int expectedDoodleSize) {
        clearInvocations(mLogoView);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(isInMultiWindowMode);
        mLogoCoordinator.updateDoodleOnTablet(showingNonStandardGoogleLogo);

        verify(mLogoView).setDoodleSize(expectedDoodleSize);
    }
}
