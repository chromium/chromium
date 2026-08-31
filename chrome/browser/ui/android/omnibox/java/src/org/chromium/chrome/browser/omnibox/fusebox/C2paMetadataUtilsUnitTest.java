// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.net.Uri;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.nio.charset.StandardCharsets;

/** Unit tests for {@link C2paMetadataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class C2paMetadataUtilsUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ContentResolver mContentResolver;

    private static final Uri TEST_URI = Uri.parse("content://test.authority/test_image.jpg");

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_markerPresent_returnsTrue() throws Exception {
        byte[] data = "sample_prefix urn:c2pa: sample_suffix".getBytes(StandardCharsets.US_ASCII);
        when(mContentResolver.openInputStream(TEST_URI)).thenReturn(new ByteArrayInputStream(data));

        assertTrue(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_markerAbsent_returnsFalse() throws Exception {
        byte[] data = "regular image bytes without provenance".getBytes(StandardCharsets.US_ASCII);
        when(mContentResolver.openInputStream(TEST_URI)).thenReturn(new ByteArrayInputStream(data));

        assertFalse(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_featureDisabled_returnsFalse() {
        assertFalse(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_markerAcrossChunkBoundary_returnsTrue() throws Exception {
        // Position "urn:c2pa:" across the 8192-byte chunk boundary (starts at byte 8190).
        byte[] prefix = new byte[8190];
        byte[] marker = "urn:c2pa:".getBytes(StandardCharsets.US_ASCII);
        byte[] suffix = new byte[100];
        byte[] data = new byte[prefix.length + marker.length + suffix.length];
        System.arraycopy(prefix, 0, data, 0, prefix.length);
        System.arraycopy(marker, 0, data, prefix.length, marker.length);
        System.arraycopy(suffix, 0, data, prefix.length + marker.length, suffix.length);

        when(mContentResolver.openInputStream(TEST_URI)).thenReturn(new ByteArrayInputStream(data));

        assertTrue(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_markerBeyondSearchLimit_returnsFalse() throws Exception {
        // Position "urn:c2pa:" beyond the 256KB search limit (starts at byte 270,000).
        byte[] prefix = new byte[270000];
        byte[] marker = "urn:c2pa:".getBytes(StandardCharsets.US_ASCII);
        byte[] data = new byte[prefix.length + marker.length];
        System.arraycopy(prefix, 0, data, 0, prefix.length);
        System.arraycopy(marker, 0, data, prefix.length, marker.length);

        when(mContentResolver.openInputStream(TEST_URI)).thenReturn(new ByteArrayInputStream(data));

        assertFalse(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_ioException_returnsFalse() throws Exception {
        when(mContentResolver.openInputStream(TEST_URI))
                .thenThrow(new FileNotFoundException("Test error"));

        assertFalse(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testHasC2paMetadata_nullStream_returnsFalse() throws Exception {
        when(mContentResolver.openInputStream(TEST_URI)).thenReturn(null);

        assertFalse(C2paMetadataUtils.hasC2paMetadata(mContentResolver, TEST_URI));
    }
}
