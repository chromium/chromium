// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.io.IOException;
import java.nio.ByteBuffer;

/** Test for {@link CronetInputStream}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class CronetInputStreamTest {
    // public to squelch lint warning about naming
    public CronetInputStream underTest;

    @Test
    @SmallTest
    public void testAvailable_closed_withoutException() throws Exception {
        underTest = new CronetInputStream(new MockHttpURLConnection());

        underTest.setResponseDataCompleted(null);

        assertThat(underTest.available()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testAvailable_closed_withException() throws Exception {
        underTest = new CronetInputStream(new MockHttpURLConnection());
        IOException expected = new IOException();
        underTest.setResponseDataCompleted(expected);

        IOException actual = assertThrows(IOException.class, underTest::available);

        assertThat(actual).isSameInstanceAs(expected);
    }

    @Test
    @SmallTest
    public void testAvailable_noReads() throws Exception {
        underTest = new CronetInputStream(new MockHttpURLConnection());

        assertThat(underTest.available()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testAvailable_everythingRead() throws Exception {
        int bytesInBuffer = 10;

        underTest = new CronetInputStream(new MockHttpURLConnection(bytesInBuffer));

        for (int i = 0; i < bytesInBuffer; i++) {
            underTest.read();
        }

        assertThat(underTest.available()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testAvailable_partiallyRead() throws Exception {
        int bytesInBuffer = 10;
        int consumed = 3;

        underTest = new CronetInputStream(new MockHttpURLConnection(bytesInBuffer));

        for (int i = 0; i < consumed; i++) {
            underTest.read();
        }

        assertThat(underTest.available()).isEqualTo(bytesInBuffer - consumed);
    }

    @Test
    @SmallTest
    public void testRead_afterDataCompleted() throws Exception {
        int bytesInBuffer = 10;
        int consumed = 3;

        underTest = new CronetInputStream(new MockHttpURLConnection(bytesInBuffer));

        for (int i = 0; i < consumed; i++) {
            underTest.read();
        }

        IOException expected = new IOException();
        underTest.setResponseDataCompleted(expected);

        IOException actual = assertThrows(IOException.class, underTest::read);

        assertThat(actual).isSameInstanceAs(expected);
    }

    private static class MockHttpURLConnection extends CronetHttpURLConnection {
        private final int mBytesToFill;
        private boolean mGetMoreDataExpected;

        MockHttpURLConnection() {
            super(null, null);
            this.mBytesToFill = 0;
            mGetMoreDataExpected = false;
        }

        MockHttpURLConnection(int bytesToFill) {
            super(null, null);
            this.mBytesToFill = bytesToFill;
            mGetMoreDataExpected = true;
        }

        @Override
        public void getMoreData(ByteBuffer buffer) {
            if (!mGetMoreDataExpected) {
                throw new IllegalStateException("getMoreData call not expected!");
            }
            mGetMoreDataExpected = false;
            for (int i = 0; i < mBytesToFill; i++) {
                buffer.put((byte) 0);
            }
        }
    }
}
