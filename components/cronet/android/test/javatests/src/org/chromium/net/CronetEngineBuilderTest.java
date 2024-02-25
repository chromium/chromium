// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.CronetProvider.PROVIDER_NAME_APP_PACKAGED;
import static org.chromium.net.CronetProvider.PROVIDER_NAME_FALLBACK;

import android.content.Context;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.common.truth.Correspondence;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests {@link CronetEngine.Builder}. */
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "These tests don't depend on Cronet's impl")
@Batch(Batch.UNIT_TESTS)
public class CronetEngineBuilderTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    /** Tests the comparison of two strings that contain versions. */
    @Test
    @SmallTest
    public void testVersionComparison() {
        assertVersionIsHigher("22.44", "22.43.12");
        assertVersionIsLower("22.43.12", "022.124");
        assertVersionIsLower("22.99", "22.100");
        assertVersionIsHigher("22.100", "22.99");
        assertVersionIsEqual("11.2.33", "11.2.33");
        assertIllegalArgumentException(null, "1.2.3");
        assertIllegalArgumentException("1.2.3", null);
        assertIllegalArgumentException("1.2.3", "1.2.3x");
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

        List<CronetProvider.ProviderInfo> orderedProviders =
                CronetEngine.Builder.getEnabledCronetProviders(
                        mTestRule.getTestFramework().getContext(),
                        Arrays.asList(
                                new CronetProvider.ProviderInfo[] {
                                    providerInfo1, providerInfo2, providerInfo3
                                }));

        // Check the result
        assertThat(orderedProviders)
                .containsExactly(providerInfo3, providerInfo1, providerInfo2)
                .inOrder();
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

        List<CronetProvider.ProviderInfo> orderedProviders =
                CronetEngine.Builder.getEnabledCronetProviders(
                        mTestRule.getTestFramework().getContext(),
                        new ArrayList<>(
                                Arrays.asList(
                                        new CronetProvider.ProviderInfo[] {
                                            providerInfo1, providerInfo2, providerInfo3
                                        })));

        Correspondence<CronetProvider.ProviderInfo, String> providerName =
                Correspondence.transforming(
                        providerInfo -> providerInfo.provider.getName(),
                        "The name of the provider");

        assertThat(orderedProviders)
                .comparingElementsUsing(providerName)
                .containsExactly(PROVIDER_NAME_APP_PACKAGED, PROVIDER_NAME_FALLBACK)
                .inOrder();
    }

    private void assertVersionIsHigher(String s1, String s2) {
        assertThat(CronetEngine.Builder.compareVersions(s1, s2)).isEqualTo(1);
    }

    private void assertVersionIsLower(String s1, String s2) {
        assertThat(CronetEngine.Builder.compareVersions(s1, s2)).isEqualTo(-1);
    }

    private void assertVersionIsEqual(String s1, String s2) {
        assertThat(CronetEngine.Builder.compareVersions(s1, s2)).isEqualTo(0);
    }

    private void assertIllegalArgumentException(String s1, String s2) {
        assertThrows(
                IllegalArgumentException.class, () -> CronetEngine.Builder.compareVersions(s1, s2));
    }

    // TODO(kapishnikov): Replace with a mock when mockito is supported.
    private static class FakeProvider extends CronetProvider {
        private final String mName;
        private final String mVersion;
        private final boolean mEnabled;

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
            return mEnabled;
        }

        @Override
        public String toString() {
            return mName;
        }
    }
}
