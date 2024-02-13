// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class HashTest {
    @Test
    @SmallTest
    public void testHashByteArray_returnsZeroIfNull() {
        assertThat(Hash.hash((byte[]) null)).isEqualTo(0L);
    }

    @Test
    @SmallTest
    public void testHashByteArray_returnsZeroIfEmpty() {
        assertThat(Hash.hash(new byte[] {})).isEqualTo(0L);
    }

    @Test
    @SmallTest
    public void testHashByteArray_returnsPositiveHash() {
        // The MD5 hash of "test" is 098f6bcd4621d373cade4e832627b4f6.
        // The first 8 bytes of that are 09 8f 6b cd 46 21 d3 73.
        // In big endian, that's 0x098f6bcd4621d373.
        assertThat(Hash.hash(new byte[] {'t', 'e', 's', 't'})).isEqualTo(0x098f6bcd4621d373L);
    }

    @Test
    @SmallTest
    public void testHashByteArray_returnsNegativeHash() {
        // The MD5 hash of "test2" is ad0234829205b9033196ba818f7a872b.
        // The first 8 bytes of that are ad 02 34 82 92 05 b9 03.
        // In big endian, that's 0xad0234829205b903.
        assertThat(Hash.hash(new byte[] {'t', 'e', 's', 't', '2'})).isEqualTo(0xad0234829205b903L);
    }

    @Test
    @SmallTest
    public void testHashString_returnsZeroIfNull() {
        assertThat(Hash.hash((String) null)).isEqualTo(0L);
    }

    @Test
    @SmallTest
    public void testHashString_returnsZeroIfEmpty() {
        assertThat(Hash.hash("")).isEqualTo(0L);
    }

    @Test
    @SmallTest
    public void testHashString_returnsPositiveHash() {
        // The MD5 hash of "test" is 098f6bcd4621d373cade4e832627b4f6.
        // The first 8 bytes of that are 09 8f 6b cd 46 21 d3 73.
        // In big endian, that's 0x098f6bcd4621d373.
        assertThat(Hash.hash("test")).isEqualTo(0x098f6bcd4621d373L);
    }

    @Test
    @SmallTest
    public void testHashString_returnsNegativeHash() {
        // The MD5 hash of "test2" is ad0234829205b9033196ba818f7a872b.
        // The first 8 bytes of that are ad 02 34 82 92 05 b9 03.
        // In big endian, that's 0xad0234829205b903.
        assertThat(Hash.hash("test2")).isEqualTo(0xad0234829205b903L);
    }
}
