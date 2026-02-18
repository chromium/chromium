// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.junit.Assume.assumeFalse;

import static org.chromium.net.CronetProvider.PROVIDER_NAME_APP_PACKAGED;
import static org.chromium.net.CronetProvider.PROVIDER_NAME_FALLBACK;

import android.content.Context;
import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.BoolFlag;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.StringFlag;
import org.chromium.net.impl.CronetLibraryLoader;
import org.chromium.net.impl.HttpEngineNativeProvider;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests {@link CronetEngine.Builder}. */
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "These tests don't depend on Cronet's impl")
@DoNotBatch(reason = "HttpFlags")
public class CronetEngineBuilderTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    /** Tests the comparison of two strings that contain versions. */
    @Test
    @SmallTest
    public void testVersionComparison() {
        assertVersionIsHigher("22.44.00", "22.43.12");
        assertVersionIsLower("22.43.12", "022.124.00");
        assertVersionIsLower("22.99", "22.100");
        assertVersionIsHigher("22.100", "22.99");
        assertVersionIsEqual("11.2.33", "11.2.33");
        assertIllegalArgumentException(null, "1.2.3");
        assertIllegalArgumentException("1.2.3", null);
        assertIllegalArgumentException("1.2.3", "1.2.3x");
    }

    @Test
    @SmallTest
    public void testCronetLibraryPreloadMustNotCrash() {
        CronetLibraryLoader.preload();
    }

    /**
     * Tests the correct ordering of the providers. The platform provider should be the last in the
     * list. Other providers should be ordered by placing providers with the higher version first.
     */
    @Test
    @SmallTest
    public void testProviderOrdering() {
        var providerInfo1 = new CronetProvider.ProviderInfo();
        providerInfo1.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.77",
                        true);
        var providerInfo2 = new CronetProvider.ProviderInfo();
        providerInfo2.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_FALLBACK,
                        "99.99",
                        true);
        var providerInfo3 = new CronetProvider.ProviderInfo();
        providerInfo3.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        "Some other provider",
                        "99.88",
                        true);

        CronetProvider.ProviderInfo orderedProviders =
                CronetEngine.Builder.getPreferredCronetProvider(
                        mTestRule.getTestFramework().getContext(),
                        Arrays.asList(
                                new CronetProvider.ProviderInfo[] {
                                    providerInfo1, providerInfo2, providerInfo3
                                }));

        // Check the result
        assertThat(orderedProviders).isEqualTo(providerInfo3);
    }

    @Test
    @SmallTest
    public void testHttpEngineProviderScoreBasedNoHttpEngineOnDevice() {
        assumeFalse(
                "This test runs only on Android devices that do not have HttpEngine available.",
                HttpEngineNativeProvider.isHttpEngineAvailable());
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.3"))
                .isEqualTo(Integer.MIN_VALUE);
    }

    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testHttpEngineProviderScoreBasedOnVersionNoFlag() {
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.3"))
                .isEqualTo(CronetProvider.PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.4"))
                .isEqualTo(CronetProvider.PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.2"))
                .isEqualTo(CronetProvider.NOT_PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetProvider.PREFERRED_MINIMUM_HTTPENGINE_VERSION_HTTP_FLAG_NAME,
                        value = "133.0.6876.4")
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testHttpEngineProviderScoreBasedOnVersionWithFlag() {
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.3"))
                .isEqualTo(CronetProvider.NOT_PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.4"))
                .isEqualTo(CronetProvider.PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
        assertThat(
                        CronetProvider.calculateHttpEngineNativeProviderScoreInternal(
                                mTestRule.getTestFramework().getContext(), "133.0.6876.2"))
                .isEqualTo(CronetProvider.NOT_PREFERRED_HTTP_ENGINE_PROVIDER_SCORE);
    }

    @Test
    @SmallTest
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = CronetEngine.USE_SCORE_BASED_PROVIDER_SELECTION_HTTP_FLAG_NAME,
                        value = true)
            })
    public void testSmartLogicSorting() {
        var providerInfo1 = new CronetProvider.ProviderInfo();
        providerInfo1.providerScore = 2;
        providerInfo1.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.99",
                        true);
        var providerInfo2 = new CronetProvider.ProviderInfo();
        providerInfo2.providerScore = 1;
        providerInfo2.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.77",
                        true);

        CronetProvider.ProviderInfo orderedProvider =
                CronetEngine.Builder.getPreferredCronetProvider(
                        mTestRule.getTestFramework().getContext(),
                        new ArrayList<>(
                                Arrays.asList(
                                        new CronetProvider.ProviderInfo[] {
                                            providerInfo2, providerInfo1
                                        })));

        assertThat(orderedProvider).isEqualTo(providerInfo1);
    }

    @Test
    @SmallTest
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = CronetEngine.USE_SCORE_BASED_PROVIDER_SELECTION_HTTP_FLAG_NAME,
                        value = true)
            })
    public void testSmartLogicSortingShouldFetchFirstEnabledOnly() {
        var providerInfo1 = new CronetProvider.ProviderInfo();
        providerInfo1.providerScore = 2;
        FakeProvider provider1 =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.99",
                        true);
        providerInfo1.provider = provider1;
        var providerInfo2 = new CronetProvider.ProviderInfo();
        providerInfo2.providerScore = 1;
        FakeProvider provider2 =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.77",
                        false);
        providerInfo2.provider = provider2;

        CronetProvider.ProviderInfo orderedProvider =
                CronetEngine.Builder.getPreferredCronetProvider(
                        mTestRule.getTestFramework().getContext(),
                        new ArrayList<>(
                                Arrays.asList(
                                        new CronetProvider.ProviderInfo[] {
                                            providerInfo2, providerInfo1
                                        })));

        assertThat(orderedProvider).isEqualTo(providerInfo1);
        assertThat(provider1.mIsEnabledCalled).isTrue();
        assertThat(provider2.mIsEnabledCalled).isFalse();
    }

    /**
     * Tests that the providers that are disabled are not included in the list of available
     * providers when the provider is selected by the default selection logic.
     */
    @Test
    @SmallTest
    public void testThatDisabledProvidersAreExcluded() {
        var providerInfo1 = new CronetProvider.ProviderInfo();
        providerInfo1.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_FALLBACK,
                        "99.99",
                        true);
        var providerInfo2 = new CronetProvider.ProviderInfo();
        providerInfo2.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        PROVIDER_NAME_APP_PACKAGED,
                        "99.77",
                        true);
        var providerInfo3 = new CronetProvider.ProviderInfo();
        providerInfo3.provider =
                new FakeProvider(
                        mTestRule.getTestFramework().getContext(),
                        "Some other provider",
                        "99.88",
                        false);

        CronetProvider.ProviderInfo orderedProvider =
                CronetEngine.Builder.getPreferredCronetProvider(
                        mTestRule.getTestFramework().getContext(),
                        new ArrayList<>(
                                Arrays.asList(
                                        new CronetProvider.ProviderInfo[] {
                                            providerInfo1, providerInfo2, providerInfo3
                                        })));

        assertThat(orderedProvider).isNotEqualTo(providerInfo3);
    }

    private void assertVersionIsHigher(String s1, String s2) {
        assertThat(CronetProvider.compareVersions(s1, s2)).isEqualTo(1);
    }

    private void assertVersionIsLower(String s1, String s2) {
        assertThat(CronetProvider.compareVersions(s1, s2)).isEqualTo(-1);
    }

    private void assertVersionIsEqual(String s1, String s2) {
        assertThat(CronetProvider.compareVersions(s1, s2)).isEqualTo(0);
    }

    private void assertIllegalArgumentException(String s1, String s2) {
        assertThrows(IllegalArgumentException.class, () -> CronetProvider.compareVersions(s1, s2));
    }

    // TODO(kapishnikov): Replace with a mock when mockito is supported.
    private static class FakeProvider extends CronetProvider {
        private final String mName;
        private final String mVersion;
        private final boolean mEnabled;
        public boolean mIsEnabledCalled;

        protected FakeProvider(Context context, String name, String version, boolean enabled) {
            super(context);
            mName = name;
            mVersion = version;
            mEnabled = enabled;
        }

        @Override
        public CronetEngine.Builder createBuilder() {
            return new CronetEngine.Builder((ICronetEngineBuilder) null);
        }

        @Override
        public String getName() {
            return mName;
        }

        @Override
        public String getVersion() {
            return mVersion;
        }

        @Override
        public boolean isEnabled() {
            mIsEnabledCalled = true;
            return mEnabled;
        }

        @Override
        public String toString() {
            return mName;
        }
    }
}
