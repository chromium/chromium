// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.search_engines.SearchEngineCountryDelegate.DeviceChoiceEventType;
import org.chromium.components.search_engines.test.util.SearchEnginesFeaturesTestUtil;

import java.time.Instant;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

@SmallTest
@RunWith(ParameterizedRobolectricTestRunner.class)
public class SearchEngineChoiceServiceUnitTest {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock Context mContext;
    private @Mock SearchEngineCountryDelegate mDelegate;

    private final boolean mIsClayBlockingEnabled;

    public SearchEngineChoiceServiceUnitTest(boolean isClayBlockingEnabled) {
        this.mIsClayBlockingEnabled = isClayBlockingEnabled;
    }

    @Before
    public void setUp() {
        FeatureList.setDisableNativeForTesting(true);
        configureClayBlockingFeature(
                mIsClayBlockingEnabled,
                /* isDarkLaunchEnabled= */ false,
                /* defaultBrowserPromoSuppressedMillis= */ null);

        doReturn(Promise.rejected()).when(mDelegate).getDeviceCountry();
        doReturn(new ObservableSupplierImpl<>(false))
                .when(mDelegate)
                .getIsDeviceChoiceRequiredSupplier();
    }

    @Test
    public void testAbstractDelegate() {
        var service = new SearchEngineChoiceService(new SearchEngineCountryDelegate(mContext) {});

        // The default implementation should be set to not trigger anything disruptive.
        assertTrue(service.getDeviceCountry().isRejected());

        assertFalse(service.isDeviceChoiceDialogEligible());
        assertFalse(service.getIsDeviceChoiceRequiredSupplier().get());
    }

    @Test
    public void testFakeDelegate() {
        var service =
                new SearchEngineChoiceService(
                        new FakeSearchEngineCountryDelegate(/* enableLogging= */ true));

        if (mIsClayBlockingEnabled) {
            // It should have generally sensible values and make the dialog be shown.
            assertTrue(service.getDeviceCountry().isFulfilled());

            assertTrue(service.isDeviceChoiceDialogEligible());

            var supplier = service.getIsDeviceChoiceRequiredSupplier();
            ShadowLooper.runUiThreadTasks();
            assertTrue(supplier.get());
        } else {
            // Same as the abstract delegate.
            assertTrue(service.getDeviceCountry().isRejected());

            assertFalse(service.isDeviceChoiceDialogEligible());

            var supplier = service.getIsDeviceChoiceRequiredSupplier();
            ShadowLooper.runUiThreadTasks();

            assertFalse(supplier.get());
        }

        // The calls below should be fine to run without triggering anything.
        service.launchDeviceChoiceScreens();
        service.notifyDeviceChoiceBlockCleared();
        service.notifyDeviceChoiceBlockShown();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    public void testGetDeviceCountry_rejected() {
        reset(mDelegate);
        doReturn(Promise.rejected()).when(mDelegate).getDeviceCountry();

        var service = new SearchEngineChoiceService(mDelegate);

        assertTrue(service.getDeviceCountry().isRejected());
        verify(mDelegate, times(1)).getDeviceCountry();

        // Even if it changes, the device country is not fetched again afterwards.
        reset(mDelegate);
        assertTrue(service.getDeviceCountry().isRejected());
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testGetDeviceCountry_fulfilled() {
        reset(mDelegate);
        doReturn(Promise.fulfilled("countryCode")).when(mDelegate).getDeviceCountry();

        var service = new SearchEngineChoiceService(mDelegate);

        var deviceCountryPromise = service.getDeviceCountry();
        assertTrue(deviceCountryPromise.isFulfilled());
        assertEquals("countryCode", deviceCountryPromise.getResult());
        verify(mDelegate, times(1)).getDeviceCountry();

        // Even if it changes, the device country is not fetched again afterwards.
        reset(mDelegate);
        assertEquals("countryCode", service.getDeviceCountry().getResult());
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testIsDeviceDialogChoiceEligible() {
        var service = new SearchEngineChoiceService(mDelegate);

        doReturn(false).when(mDelegate).isDeviceChoiceDialogEligible();
        assertFalse(service.isDeviceChoiceDialogEligible());
        verify(mDelegate, times(mIsClayBlockingEnabled ? 1 : 0)).isDeviceChoiceDialogEligible();

        doReturn(true).when(mDelegate).isDeviceChoiceDialogEligible();
        if (mIsClayBlockingEnabled) {
            assertTrue(service.isDeviceChoiceDialogEligible());
            verify(mDelegate, times(2)).isDeviceChoiceDialogEligible();
        } else {
            assertFalse(service.isDeviceChoiceDialogEligible());
            verify(mDelegate, never()).isDeviceChoiceDialogEligible();
        }
    }

    @Test
    public void testGetIsDeviceChoiceRequiredSupplier() {
        ObservableSupplier<Boolean> fakeSupplier = new ObservableSupplierImpl<>();
        doReturn(fakeSupplier).when(mDelegate).getIsDeviceChoiceRequiredSupplier();

        var service = new SearchEngineChoiceService(mDelegate);

        var actualSupplier = service.getIsDeviceChoiceRequiredSupplier();

        if (mIsClayBlockingEnabled) {
            assertSame(fakeSupplier, actualSupplier);
            verify(mDelegate).getIsDeviceChoiceRequiredSupplier();
        } else {
            assertNotSame(fakeSupplier, actualSupplier);
            assertFalse(actualSupplier.get());
            verify(mDelegate, never()).getIsDeviceChoiceRequiredSupplier();
        }
    }

    @Test
    public void testGetIsDeviceChoiceRequiredSupplier_darkLaunch() {
        configureClayBlockingFeature(
                mIsClayBlockingEnabled,
                /* isDarkLaunchEnabled= */ true,
                /* defaultBrowserPromoSuppressedMillis= */ null);
        ObservableSupplierImpl<Boolean> fakeSupplier = new ObservableSupplierImpl<>();
        doReturn(fakeSupplier).when(mDelegate).getIsDeviceChoiceRequiredSupplier();

        var service = new SearchEngineChoiceService(mDelegate);
        var actualSupplier = service.getIsDeviceChoiceRequiredSupplier();

        if (mIsClayBlockingEnabled) {
            // For dark launch, we do call into the delegate, but we don't return its values
            // directly.
            assertNotSame(fakeSupplier, actualSupplier);
            assertNull(actualSupplier.get());
            verify(mDelegate).getIsDeviceChoiceRequiredSupplier();

            // We match behaviour for the pending states, but when we get a value from the delegate,
            // we ignore it and always return false.
            fakeSupplier.set(true);
            assertFalse(actualSupplier.get());
        } else {
            assertNotSame(fakeSupplier, actualSupplier);
            assertFalse(actualSupplier.get());
            verify(mDelegate, never()).getIsDeviceChoiceRequiredSupplier();
        }
    }

    @Test
    public void testNotifyDeviceChoiceBlockShown() {
        var service = new SearchEngineChoiceService(mDelegate);

        service.notifyDeviceChoiceBlockShown();
        verify(mDelegate, times(mIsClayBlockingEnabled ? 1 : 0))
                .notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_SHOWN);
    }

    @Test
    public void testNotifyDeviceChoiceBlockCleared() {
        var service = new SearchEngineChoiceService(mDelegate);

        service.notifyDeviceChoiceBlockCleared();
        verify(mDelegate, times(mIsClayBlockingEnabled ? 1 : 0))
                .notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_CLEARED);
    }

    @Test
    public void testIsDefaultBrowserPromoSuppressed() {
        final int defaultBrowserPromoSuppressedMillis = 24_000;
        // Param state: A suppression period is specified.
        configureClayBlockingFeature(
                mIsClayBlockingEnabled,
                /* isDarkLaunchEnabled= */ false,
                defaultBrowserPromoSuppressedMillis);

        {
            // Default browser selection did not happen => promo should not be suppressed.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(null).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertFalse(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Default browser selection happened a long time ago => promo should not be suppressed.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(Instant.MIN).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertFalse(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Selection date + suppression period overflow => promo should not be suppressed.
            // Would indicate a configuration issue.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(Instant.MAX).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertFalse(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Default browser selection happened too recently (simulated by being a date in the
            // future) => promo should be suppressed if the feature is enabled.
            Instant futureInstant = Instant.now().plusMillis(defaultBrowserPromoSuppressedMillis);
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(futureInstant).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertEquals(mIsClayBlockingEnabled, service.isDefaultBrowserPromoSuppressed());
        }

        // Param state: An invalid suppression period is specified.
        configureClayBlockingFeature(
                mIsClayBlockingEnabled,
                /* isDarkLaunchEnabled= */ false,
                /* defaultBrowserPromoSuppressedMillis= */ -24);
        {
            // Recent selection but invalid suppression period => promo should not be suppressed.
            Instant futureInstant = Instant.now().plusMillis(defaultBrowserPromoSuppressedMillis);
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(futureInstant).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertFalse(service.isDefaultBrowserPromoSuppressed());
        }
    }

    private static void configureClayBlockingFeature(
            boolean isClayBlockingEnabled,
            boolean isDarkLaunchEnabled,
            @Nullable Integer defaultBrowserPromoSuppressedMillis) {
        if (isClayBlockingEnabled) {
            Map<String, String> params = new HashMap<>();
            params.put("is_dark_launch", isDarkLaunchEnabled ? "true" : "");
            params.put("dialog_timeout_millis", "0");
            if (defaultBrowserPromoSuppressedMillis != null) {
                params.put(
                        "default_browser_promo_suppressed_millis",
                        defaultBrowserPromoSuppressedMillis.toString());
            }
            SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(params);
        } else {
            FeatureList.setTestFeatures(Map.of(SearchEnginesFeatures.CLAY_BLOCKING, false));
        }
    }
}
