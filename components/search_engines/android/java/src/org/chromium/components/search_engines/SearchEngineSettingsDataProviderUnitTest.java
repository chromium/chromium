// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link SearchEngineSettingsDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineSettingsDataProviderUnitTest {
    private static final long NATIVE_SERVICE_PTR = 100L;
    private static final long NATIVE_PROVIDER_PTR = 200L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TemplateUrlService.Natives mTemplateUrlServiceJni;
    @Mock private SearchEngineSettingsDataProvider.Natives mProviderJni;

    private TemplateUrlService mTemplateUrlService;

    @Before
    public void setUp() {
        TemplateUrlServiceJni.setInstanceForTesting(mTemplateUrlServiceJni);
        SearchEngineSettingsDataProviderJni.setInstanceForTesting(mProviderJni);
        when(mTemplateUrlServiceJni.createSettingsDataProvider(NATIVE_SERVICE_PTR))
                .thenReturn(NATIVE_PROVIDER_PTR);
        mTemplateUrlService = TemplateUrlService.create(NATIVE_SERVICE_PTR);
    }

    @Test
    public void testGetTemplateUrlsByCategoryAndClose() {
        TemplateUrl expectedUrl = mock(TemplateUrl.class);
        when(mProviderJni.getTemplateUrlsByCategory(
                        NATIVE_PROVIDER_PTR, TemplateUrlCategory.DEFAULT))
                .thenReturn(List.of(expectedUrl));

        try (SearchEngineSettingsDataProvider provider =
                mTemplateUrlService.createSettingsDataProvider()) {
            verify(mTemplateUrlServiceJni).createSettingsDataProvider(NATIVE_SERVICE_PTR);

            List<TemplateUrl> urls =
                    provider.getTemplateUrlsByCategory(TemplateUrlCategory.DEFAULT);
            assertEquals(List.of(expectedUrl), urls);

            // Calling close() multiple times is idempotent and only destroys native once.
            provider.close();
        }

        verify(mProviderJni, times(1)).destroy(NATIVE_PROVIDER_PTR);
    }

    @Test
    public void testGetPrepopulatedAndRecentlyVisitedTemplateURLs() {
        PrepopulatedAndRecentlyVisitedTemplateURLs expected =
                new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(), List.of());
        when(mProviderJni.getPrepopulatedAndRecentlyVisitedTemplateURLs(NATIVE_PROVIDER_PTR))
                .thenReturn(expected);

        try (SearchEngineSettingsDataProvider provider =
                mTemplateUrlService.createSettingsDataProvider()) {
            assertEquals(expected, provider.getPrepopulatedAndRecentlyVisitedTemplateURLs());
        }
    }

    @Test
    public void testMaybeRecordSettingsPageLoadMetrics() {
        TemplateUrl t1 = mock(TemplateUrl.class);
        TemplateUrl t2 = mock(TemplateUrl.class);
        when(t1.getId()).thenReturn(11L);
        when(t2.getId()).thenReturn(22L);

        try (SearchEngineSettingsDataProvider provider =
                mTemplateUrlService.createSettingsDataProvider()) {
            provider.maybeRecordSettingsPageLoadMetrics(List.of(t1), List.of(t2));
        }

        verify(mProviderJni)
                .maybeRecordSettingsPageLoadMetrics(NATIVE_PROVIDER_PTR, new long[] {11L, 22L});
    }
}
