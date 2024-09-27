// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import org.chromium.net.UrlResponseInfo;

import java.io.UnsupportedEncodingException;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;

// TODO(kirchman): Update this to explain inter-class usage once other classes land.
/**
 *
 * Fake response model for UrlRequest used by Fake Cronet.
 */
public class FakeUrlResponse {
    private final int mHttpStatusCode;
    // Entries to mAllHeadersList should never be mutated.
    private final List<Map.Entry<String, String>> mAllHeadersList;
    private final boolean mWasCached;
    private final String mNegotiatedProtocol;
    private final String mProxyServer;
    private final byte[] mResponseBody;

    private static <T> T getNullableOrDefault(T nullableObject, T defaultObject) {
        if (nullableObject != null) {
            return nullableObject;
        }
        return defaultObject;
    }

    /**
     * Constructs a {@link FakeUrlResponse} from a {@link FakeUrlResponse.Builder}.
     * @param builder the {@link FakeUrlResponse.Builder} to create the response from
     */
    private FakeUrlResponse(Builder builder) {
        mHttpStatusCode = builder.mHttpStatusCode;
        mAllHeadersList = Collections.unmodifiableList(new ArrayList<>(builder.mAllHeadersList));
        mWasCached = builder.mWasCached;
        mNegotiatedProtocol = builder.mNegotiatedProtocol;
        mProxyServer = builder.mProxyServer;
        mResponseBody = builder.mResponseBody;
    }

    /**
     * Constructs a {@link FakeUrlResponse} from a {@link UrlResponseInfo}. All nullable fields in
     * the {@link UrlResponseInfo} are initialized to the default value if the provided value is
     * null.
     *
     * @param info the {@link UrlResponseInfo} used to initialize this object's fields
     */
    public FakeUrlResponse(UrlResponseInfo info) {
        mHttpStatusCode = info.getHttpStatusCode();
        mAllHeadersList = Collections.unmodifiableList(new ArrayList<>(info.getAllHeadersAsList()));
        mWasCached = info.wasCached();
        mNegotiatedProtocol =
                getNullableOrDefault(
                        info.getNegotiatedProtocol(), Builder.DEFAULT_NEGOTIATED_PROTOCOL);
        mProxyServer = getNullableOrDefault(info.getProxyServer(), Builder.DEFAULT_PROXY_SERVER);
        mResponseBody = Builder.DEFAULT_RESPONSE_BODY;
    }

    /** Builds a {@link FakeUrlResponse}. */
    public static class Builder {
        private static final int DEFAULT_HTTP_STATUS_CODE = 200;
        private static final List<Map.Entry<String, String>> INTERNAL_INITIAL_HEADERS_LIST =
                new ArrayList<>();
        private static final boolean DEFAULT_WAS_CACHED = false;
        private static final String DEFAULT_NEGOTIATED_PROTOCOL = "";
        private static final String DEFAULT_PROXY_SERVER = "";
        private static final byte[] DEFAULT_RESPONSE_BODY = new byte[0];

        private int mHttpStatusCode = DEFAULT_HTTP_STATUS_CODE;
        // Entries to mAllHeadersList should never be mutated.
        private List<Map.Entry<String, String>> mAllHeadersList =
                new ArrayList<>(INTERNAL_INITIAL_HEADERS_LIST);
        private boolean mWasCached = DEFAULT_WAS_CACHED;
        private String mNegotiatedProtocol = DEFAULT_NEGOTIATED_PROTOCOL;
        private String mProxyServer = DEFAULT_PROXY_SERVER;
        private byte[] mResponseBody = DEFAULT_RESPONSE_BODY;

        /** Constructs a {@link FakeUrlResponse.Builder} with the default parameters. */
        public Builder() {}

        /**
         * Constructs a {@link FakeUrlResponse.Builder} from a source {@link FakeUrlResponse}.
         *
         * @param source a {@link FakeUrlResponse} to copy into this {@link FakeUrlResponse.Builder}
         */
        private Builder(FakeUrlResponse source) {
            mHttpStatusCode = source.getHttpStatusCode();
            mAllHeadersList = new ArrayList<>(source.getAllHeadersList());
            mWasCached = source.getWasCached();
            mNegotiatedProtocol = source.getNegotiatedProtocol();
            mProxyServer = source.getProxyServer();
            mResponseBody = source.getResponseBody();
        }

        /**
         * Sets the HTTP status code. The default value is 200.
         *
         * @param httpStatusCode for {@link UrlResponseInfo.getHttpStatusCode()}
         * @return the builder with the corresponding HTTP status code set
         */
        public Builder setHttpStatusCode(int httpStatusCode) {
            mHttpStatusCode = httpStatusCode;
            return this;
        }

        /**
         * Adds a response header to built {@link FakeUrlResponse}s.
         *
         * @param name  the name of the header key, for example, "location" for a redirect header
         * @param value the header value
         * @return the builder with the corresponding header set
         */
        public Builder addHeader(String name, String value) {
            mAllHeadersList.add(new AbstractMap.SimpleEntry<>(name, value));
            return this;
        }

        /**
         * Sets result of {@link UrlResponseInfo.wasCached()}. The default wasCached value is false.
         *
         * @param wasCached for {@link UrlResponseInfo.wasCached()}
         * @return the builder with the corresponding wasCached field set
         */
        public Builder setWasCached(boolean wasCached) {
            mWasCached = wasCached;
            return this;
        }

        /**
         * Sets result of {@link UrlResponseInfo.getNegotiatedProtocol()}. The default negotiated
         * protocol is an empty string.
         *
         * @param negotiatedProtocol for {@link UrlResponseInfo.getNegotiatedProtocol()}
         * @return the builder with the corresponding negotiatedProtocol field set
         */
        public Builder setNegotiatedProtocol(String negotiatedProtocol) {
            mNegotiatedProtocol = negotiatedProtocol;
            return this;
        }

        /**
         * Sets result of {@link UrlResponseInfo.getProxyServer()}. The default proxy server is an
         * empty string.
         *
         * @param proxyServer for {@link UrlResponseInfo.getProxyServer()}
         * @return the builder with the corresponding proxyServer field set
         */
        public Builder setProxyServer(String proxyServer) {
            mProxyServer = proxyServer;
            return this;
        }

        /**
         * Sets the response body for a response. The default response body is an empty byte array.
         *
         * @param body all the information the server returns
         * @return the builder with the corresponding responseBody field set
         */
        public Builder setResponseBody(byte[] body) {
            mResponseBody = body;
            return this;
        }

        /**
         * Constructs a {@link FakeUrlResponse} from this {@link FakeUrlResponse.Builder}.
         *
         * @return a FakeUrlResponse with all fields set according to this builder
         */
        public FakeUrlResponse build() {
            return new FakeUrlResponse(this);
        }
    }

    /**
     * Returns the HTTP status code.
     *
     * @return the HTTP status code.
     */
    int getHttpStatusCode() {
        return mHttpStatusCode;
    }

    /**
     * Returns an unmodifiable list of the response header key and value pairs.
     *
     * @return an unmodifiable list of response header key and value pairs
     */
    List<Map.Entry<String, String>> getAllHeadersList() {
        return mAllHeadersList;
    }

    /**
     * Returns the wasCached value for this response.
     *
     * @return the wasCached value for this response
     */
    boolean getWasCached() {
        return mWasCached;
    }

    /**
     * Returns the protocol (for example 'quic/1+spdy/3') negotiated with the server.
     *
     * @return the protocol negotiated with the server
     */
    String getNegotiatedProtocol() {
        return mNegotiatedProtocol;
    }

    /**
     * Returns the proxy server that was used for the request.
     *
     * @return the proxy server that was used for the request
     */
    String getProxyServer() {
        return mProxyServer;
    }

    /**
     * Returns the body of the response as a byte array. Used for {@link UrlRequest.Callback}
     * {@code read()} callback.
     *
     * @return the response body
     */
    byte[] getResponseBody() {
        return mResponseBody;
    }

    /**
     * Returns a mutable builder representation of this {@link FakeUrlResponse}
     *
     * @return a {@link FakeUrlResponse.Builder} with all fields copied from this instance.
     */
    public Builder toBuilder() {
        return new Builder(this);
    }

    @Override
    public boolean equals(Object otherObj) {
        if (!(otherObj instanceof FakeUrlResponse)) {
            return false;
        }
        FakeUrlResponse other = (FakeUrlResponse) otherObj;
        return (mHttpStatusCode == other.mHttpStatusCode
                && mAllHeadersList.equals(other.mAllHeadersList)
                && mWasCached == other.mWasCached
                && mNegotiatedProtocol.equals(other.mNegotiatedProtocol)
                && mProxyServer.equals(other.mProxyServer)
                && Arrays.equals(mResponseBody, other.mResponseBody));
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mHttpStatusCode,
                mAllHeadersList,
                mWasCached,
                mNegotiatedProtocol,
                mProxyServer,
                Arrays.hashCode(mResponseBody));
    }

    @Override
    public String toString() {
        StringBuilder outputString = new StringBuilder();
        outputString.append("HTTP Status Code: " + mHttpStatusCode);
        outputString.append(" Headers: " + mAllHeadersList.toString());
        outputString.append(" Was Cached: " + mWasCached);
        outputString.append(" Negotiated Protocol: " + mNegotiatedProtocol);
        outputString.append(" Proxy Server: " + mProxyServer);
        outputString.append(" Response Body ");
        try {
            String bodyString = new String(mResponseBody, "UTF-8");
            outputString.append("(UTF-8): " + bodyString);
        } catch (UnsupportedEncodingException e) {
            outputString.append("(hexadecimal): " + getHexStringFromBytes(mResponseBody));
        }
        return outputString.toString();
    }

    private String getHexStringFromBytes(byte[] bytes) {
        StringBuilder bytesToHexStringBuilder = new StringBuilder();
        for (byte b : mResponseBody) {
            bytesToHexStringBuilder.append(String.format("%02x", b));
        }
        return bytesToHexStringBuilder.toString();
    }
}
