// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.base.test.util.Matchers.fulfilledPromise;
import static org.chromium.base.test.util.Matchers.rejectedPromise;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.search_engines.SearchEngineChoiceService.RefreshReason;
import org.chromium.components.search_engines.SearchEngineCountryDelegate.DefaultBrowserPromoSuppressionDelayType;
import org.chromium.components.search_engines.SearchEngineCountryDelegate.DeviceChoiceEventType;

import java.time.Instant;
import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineChoiceServiceUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock SearchEngineCountryDelegate mDelegate;

    @Before
    public void setUp() {
        FeatureList.setDisableNativeForTesting(true);
        doReturn(Promise.rejected()).when(mDelegate).getDeviceCountry();
        doReturn(new ObservableSupplierImpl<>(false))
                .when(mDelegate)
                .getIsDeviceChoiceRequiredSupplier();
    }

    @Test
    public void testAbstractDelegate() {
        var service = new SearchEngineChoiceService(new SearchEngineCountryDelegate() {});

        // The default implementation should be set to not trigger anything disruptive.
        assertThat(service.getDeviceCountry(), is(rejectedPromise()));

        assertFalse(service.isDeviceChoiceDialogEligible());
        assertFalse(service.getIsDeviceChoiceRequiredSupplier().get());
        assertFalse(service.isDefaultBrowserPromoSuppressed());

        service.notifyDeviceChoiceBlockShown();
        service.launchDeviceChoiceScreens();
        service.refreshDeviceChoiceRequiredNow(RefreshReason.APP_RESUME);
        service.notifyDeviceChoiceBlockCleared();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    public void testFakeDelegate() {
        var service =
                new SearchEngineChoiceService(
                        new FakeSearchEngineCountryDelegate(/* enableLogging= */ true));

        // It should have generally sensible values and make the dialog be shown.
        assertThat(service.getDeviceCountry(), is(fulfilledPromise()));

        assertTrue(service.isDeviceChoiceDialogEligible());

        var supplier = service.getIsDeviceChoiceRequiredSupplier();
        ShadowLooper.runUiThreadTasks();
        ShadowLooper.idleMainLooper(3000, TimeUnit.MILLISECONDS);
        assertTrue(supplier.get());

        // The calls below should be fine to run without triggering anything.
        assertFalse(service.isDefaultBrowserPromoSuppressed());
        service.notifyDeviceChoiceBlockShown();
        service.launchDeviceChoiceScreens();
        service.refreshDeviceChoiceRequiredNow(RefreshReason.APP_RESUME);
        service.notifyDeviceChoiceBlockCleared();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    public void testGetDeviceCountry_rejected() {
        reset(mDelegate);
        doReturn(Promise.rejected()).when(mDelegate).getDeviceCountry();

        var service = new SearchEngineChoiceService(mDelegate);

        assertThat(service.getDeviceCountry(), is(rejectedPromise()));
        verify(mDelegate).getDeviceCountry();

        // Even if it changes, the device country is not fetched again afterwards.
        reset(mDelegate);
        assertThat(service.getDeviceCountry(), is(rejectedPromise()));
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testGetDeviceCountry_fulfilled() {
        reset(mDelegate);
        doReturn(Promise.fulfilled("countryCode")).when(mDelegate).getDeviceCountry();

        var service = new SearchEngineChoiceService(mDelegate);

        assertThat(service.getDeviceCountry(), is(fulfilledPromise(equalTo("countryCode"))));
        verify(mDelegate).getDeviceCountry();

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
        verify(mDelegate).isDeviceChoiceDialogEligible();

        doReturn(true).when(mDelegate).isDeviceChoiceDialogEligible();
        assertTrue(service.isDeviceChoiceDialogEligible());
        verify(mDelegate, times(2)).isDeviceChoiceDialogEligible();
    }

    @Test
    public void testGetIsDeviceChoiceRequiredSupplier() {
        ObservableSupplier<Boolean> fakeSupplier = new ObservableSupplierImpl<>();
        doReturn(fakeSupplier).when(mDelegate).getIsDeviceChoiceRequiredSupplier();

        var service = new SearchEngineChoiceService(mDelegate);

        var actualSupplier = service.getIsDeviceChoiceRequiredSupplier();

        assertSame(fakeSupplier, actualSupplier);
        verify(mDelegate).getIsDeviceChoiceRequiredSupplier();
    }

    @Test
    public void testNotifyDeviceChoiceBlockShown() {
        var service = new SearchEngineChoiceService(mDelegate);

        service.notifyDeviceChoiceBlockShown();
        verify(mDelegate).notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_SHOWN);
    }

    @Test
    public void testNotifyDeviceChoiceBlockCleared() {
        var service = new SearchEngineChoiceService(mDelegate);

        service.notifyDeviceChoiceBlockCleared();
        verify(mDelegate).notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_CLEARED);
    }

    @Test
    public void testIsDefaultBrowserPromoSuppressed_suppressIfChoiceSet_maxDelay() {
        doReturn(DefaultBrowserPromoSuppressionDelayType.MAX)
                .when(mDelegate)
                .getDefaultBrowserPromoSuppressionDelayType();

        {
            // Default browser selection did not happen => promo should not be suppressed.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(null).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertFalse(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Default browser selection happened a long time ago => promo should be suppressed.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(Instant.MIN).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertTrue(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Selection date + suppression period overflow => promo should be suppressed.
            // Would indicate a configuration issue.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(Instant.MAX).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertTrue(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Default browser selection happened too recently (simulated by being a date in the
            // future) => promo should be suppressed if the feature is enabled.
            Instant futureInstant =
                    Instant.now()
                            .plusMillis(
                                    SearchEnginesFeatureUtils
                                            .CHOICE_DIALOG_DEFAULT_BROWSER_PROMO_SUPPRESSED_MILLIS);
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(futureInstant).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertTrue(service.isDefaultBrowserPromoSuppressed());
        }
    }

    @Test
    public void testIsDefaultBrowserPromoSuppressed_suppressIfChoiceSet_standardDelay() {
        doReturn(DefaultBrowserPromoSuppressionDelayType.STANDARD)
                .when(mDelegate)
                .getDefaultBrowserPromoSuppressionDelayType();

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
            // Selection date + suppression period overflow => promo should be suppressed.
            // Would indicate a configuration issue.
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(Instant.MAX).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertTrue(service.isDefaultBrowserPromoSuppressed());
        }

        {
            // Default browser selection happened too recently (simulated by being a date in the
            // future) => promo should be suppressed if the feature is enabled.
            Instant futureInstant =
                    Instant.now()
                            .plusMillis(
                                    SearchEnginesFeatureUtils
                                            .CHOICE_DIALOG_DEFAULT_BROWSER_PROMO_SUPPRESSED_MILLIS);
            var service = new SearchEngineChoiceService(mDelegate);
            doReturn(futureInstant).when(mDelegate).getDeviceBrowserSelectedTimestamp();
            assertTrue(service.isDefaultBrowserPromoSuppressed());
        }
    }

    @Test
    public void testDelegateRelease() {
        var deviceCountryPromise = new Promise<String>();
        doReturn(deviceCountryPromise).when(mDelegate).getDeviceCountry();
        doReturn(false).when(mDelegate).isDeviceChoiceDialogEligible();
        doReturn(Instant.now()).when(mDelegate).getDeviceBrowserSelectedTimestamp();
        doReturn(DefaultBrowserPromoSuppressionDelayType.MAX)
                .when(mDelegate)
                .getDefaultBrowserPromoSuppressionDelayType();

        var service = new SearchEngineChoiceService(mDelegate);

        // Device country is being checked on startup.
        verify(mDelegate).getDeviceCountry();

        service.isDeviceChoiceDialogEligible();
        verify(mDelegate).isDeviceChoiceDialogEligible();

        // Eligibility is checked every time, not cached.
        service.isDeviceChoiceDialogEligible();
        verify(mDelegate, times(2)).isDeviceChoiceDialogEligible();

        // On resolution, the delegate is freed up, but the default browser selection timestamp is
        // proactively checked, in case we need it later.
        deviceCountryPromise.fulfill("deviceCountry");
        ShadowLooper.runUiThreadTasks();
        verify(mDelegate).getDeviceBrowserSelectedTimestamp();

        // The delegate is not checked anymore, because it was freed up.
        clearInvocations(mDelegate);
        service.isDeviceChoiceDialogEligible();
        verify(mDelegate, never()).isDeviceChoiceDialogEligible();

        // We still can check whether the default browser promo is suppressed.
        assertTrue(service.isDefaultBrowserPromoSuppressed());
    }

    @Test
    public void testDelegateRelease_blocked() {
        var deviceCountryPromise = new Promise<String>();
        doReturn(deviceCountryPromise).when(mDelegate).getDeviceCountry();
        doReturn(true).when(mDelegate).isDeviceChoiceDialogEligible();

        var service = new SearchEngineChoiceService(mDelegate);

        service.isDeviceChoiceDialogEligible();
        verify(mDelegate).isDeviceChoiceDialogEligible();

        // On resolution, the delegate is not freed up, so we don't need to check the default
        // browser selection timestamp.
        deviceCountryPromise.fulfill("deviceCountry");
        ShadowLooper.runUiThreadTasks();
        verify(mDelegate, never()).getDeviceBrowserSelectedTimestamp();

        // The delegate is still checked, the service kept it.
        clearInvocations(mDelegate);
        service.isDeviceChoiceDialogEligible();
        verify(mDelegate).isDeviceChoiceDialogEligible();
    }
}
