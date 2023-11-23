// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.junit.Assert;

import org.chromium.base.ApiCompatibilityUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * A HttpURLConnection that performs some basic checks to ensure we are uploading
 * minidumps correctly.
 */
public class TestHttpURLConnection extends HttpURLConnection {
    static final String DEFAULT_EXPECTED_CONTENT_TYPE =
            String.format(
                    MinidumpUploader.CONTENT_TYPE_TMPL, MinidumpUploaderTestConstants.BOUNDARY);
    private final String mExpectedContentType;

    /** The value of the "Content-Type" property if the property has been set. */
    private String mContentTypePropertyValue = "";

    public TestHttpURLConnection(URL url) {
        this(url, DEFAULT_EXPECTED_CONTENT_TYPE);
    }

    public TestHttpURLConnection(URL url, String contentType) {
        super(url);
        mExpectedContentType = contentType;
        Assert.assertEquals(MinidumpUploader.CRASH_URL_STRING, url.toString());
    }

    @Override
    public void disconnect() {
        // Check that the "Content-Type" property has been set and the property's value.
        Assert.assertEquals(mExpectedContentType, mContentTypePropertyValue);
    }

    @Override
    public InputStream getInputStream() {
        return new ByteArrayInputStream(
                ApiCompatibilityUtils.getBytesUtf8(MinidumpUploaderTestConstants.UPLOAD_CRASH_ID));
    }

    @Override
    public OutputStream getOutputStream() {
        return new ByteArrayOutputStream();
    }

    @Override
    public int getResponseCode() {
        return 200;
    }

    @Override
    public String getResponseMessage() {
        return null;
    }

    @Override
    public boolean usingProxy() {
        return false;
    }

    @Override
    public void connect() {}

    @Override
    public void setRequestProperty(String key, String value) {
        if (key.equals("Content-Type")) {
            mContentTypePropertyValue = value;
        }
    }
}
