// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
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
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

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
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoUtils.DoodleSize;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.PaddingStyle;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.function.Supplier;

/** Unit tests for the {@link LogoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LogoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    private LogoContainerView mLogoContainerView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(false);
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
        NtpBackgroundDataColor dataColor =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromNtpBackgroundData(mContext, dataColor);

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
        dataColor =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromNtpBackgroundData(mContext, dataColor);

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
        assertEquals(DoodleSize.REGULAR, mLogoContainerView.getDoodleSizeForTesting());

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
        assertEquals(DoodleSize.REGULAR, mLogoContainerView.getDoodleSizeForTesting());

        // Preset sentinel values so that the update below is observable.
        LogoView logoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        logoView.setLogoHeight(1);
        logoView.setLogoTopMargin(1);

        // Tablet transitions to multi-window mode.
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(true);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ true);
        int[] expectedParams =
                LogoUtils.getLogoViewLayoutParams(
                        mContext.getResources(),
                        /* isLogoDoodle= */ true,
                        DoodleSize.TABLET_SPLIT_SCREEN);
        MarginLayoutParams layoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        assertEquals(expectedParams[0], layoutParams.height);
        assertEquals(expectedParams[1], layoutParams.topMargin);
    }

    @Test
    public void testUpdateDoodleOnTablet_sameMode() {
        mLogoCoordinator = createLogoCoordinator();
        assertEquals(DoodleSize.REGULAR, mLogoContainerView.getDoodleSizeForTesting());

        // Tablet mode doesn't change. Preset a different value on the view directly to verify that
        // the doodle size on the view is left untouched.
        mLogoContainerView.setDoodleSize(DoodleSize.TABLET_SPLIT_SCREEN);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        assertEquals(DoodleSize.TABLET_SPLIT_SCREEN, mLogoContainerView.getDoodleSizeForTesting());

        // Tablet transitions to multi-window mode.
        mLogoContainerView.setDoodleSize(DoodleSize.REGULAR);
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(true);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        assertEquals(DoodleSize.TABLET_SPLIT_SCREEN, mLogoContainerView.getDoodleSizeForTesting());

        // Tablet mode doesn't change.
        mLogoContainerView.setDoodleSize(DoodleSize.REGULAR);
        mLogoCoordinator.updateDoodleOnTablet(/* showingNonStandardGoogleLogo= */ false);
        assertEquals(DoodleSize.REGULAR, mLogoContainerView.getDoodleSizeForTesting());
    }

    @Test
    public void testConstructor_auroraPaddingStyleMediumOrLarge_onPhones() {
        verifyLogoTopPadding(PaddingStyle.MEDIUM, /* expectPaddingSet= */ true);
        verifyLogoTopPadding(PaddingStyle.LARGE, /* expectPaddingSet= */ true);
    }

    @Test
    public void testConstructor_auroraPaddingStyleSmallOrDefault_onPhones() {
        verifyLogoTopPadding(PaddingStyle.SMALL, /* expectPaddingSet= */ false);
        verifyLogoTopPadding(PaddingStyle.DEFAULT, /* expectPaddingSet= */ false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testConstructor_auroraPaddingStyleMediumOrLarge_onTablets() {
        verifyLogoTopPadding(PaddingStyle.MEDIUM, /* expectPaddingSet= */ false);
        verifyLogoTopPadding(PaddingStyle.LARGE, /* expectPaddingSet= */ false);
    }

    /**
     * Creates a new {@link LogoCoordinator} with a fresh parent view containing the logo ViewStub,
     * and stores the inflated {@link LogoContainerView} in {@link #mLogoContainerView}.
     */
    private LogoCoordinator createLogoCoordinator() {
        FrameLayout parentView = new FrameLayout(mContext);
        ViewStub stub = new ViewStub(mContext, R.layout.logo_view_layout);
        stub.setId(R.id.logo_view_stub);
        parentView.addView(stub);

        LogoCoordinator coordinator =
                new LogoCoordinator(
                        mContext,
                        mLogoClickedCallback,
                        parentView,
                        mOnLogoAvailableCallback,
                        mVisibilityObserver,
                        mIsInMultiWindowModeSupplier);
        coordinator.setMediatorForTesting(mLogoMediator);
        mLogoContainerView = parentView.findViewById(R.id.logo_container_view);
        return coordinator;
    }

    private void verifyDoodleSize(
            boolean isInMultiWindowMode,
            boolean showingNonStandardGoogleLogo,
            int expectedDoodleSize) {
        when(mIsInMultiWindowModeSupplier.get()).thenReturn(isInMultiWindowMode);
        mLogoCoordinator.updateDoodleOnTablet(showingNonStandardGoogleLogo);

        assertEquals(expectedDoodleSize, mLogoContainerView.getDoodleSizeForTesting());
    }

    private void verifyLogoTopPadding(@PaddingStyle int paddingStyle, boolean expectPaddingSet) {
        FeatureOverrides.overrideParam(ChromeFeatureList.NTP_AURORA, "padding_style", paddingStyle);

        createLogoCoordinator();

        LogoView logoView = mLogoContainerView.findViewById(R.id.search_provider_logo);
        int defaultPaddingTop =
                mContext.getResources().getDimensionPixelSize(R.dimen.ntp_logo_padding_top);
        assertNotEquals(0, defaultPaddingTop);
        assertEquals(expectPaddingSet ? 0 : defaultPaddingTop, logoView.getPaddingTop());
    }
}
