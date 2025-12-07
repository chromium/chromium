// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.net.test.ServerCertificate;

import java.util.List;
import java.util.Map;
import java.util.Objects;

/** Java wrapper for net::EmbeddedTestServer. */
@JNINamespace("cronet")
public final class NativeTestServer implements AutoCloseable {
    // This variable contains the response body of a request to getSuccessURL().
    public static final String SUCCESS_BODY = "this is a text file\n";

    private Long mEmbeddedTestServerAdapter;

    private NativeTestServer(
            Context context, boolean useHttps, @ServerCertificate int serverCertificate) {
        TestFilesInstaller.installIfNeeded(context);
        mEmbeddedTestServerAdapter =
                NativeTestServerJni.get()
                        .create(
                                TestFilesInstaller.getInstalledPath(context),
                                UrlUtils.getIsolatedTestRoot(),
                                useHttps,
                                serverCertificate);
    }

    public static NativeTestServer createNativeTestServer(Context context) {
        return new NativeTestServer(context, false /*  useHttps */, ServerCertificate.CERT_OK);
    }

    public static NativeTestServer createNativeTestServerWithHTTPS(
            Context context, @ServerCertificate int serverCertificate) {
        return new NativeTestServer(context, true /*  useHttps */, serverCertificate);
    }

    public void start() {
        NativeTestServerJni.get().start(mEmbeddedTestServerAdapter);
    }

    @Override
    public void close() {
        if (mEmbeddedTestServerAdapter == null) {
            return;
        }
        NativeTestServerJni.get().destroy(mEmbeddedTestServerAdapter);
        mEmbeddedTestServerAdapter = null;
    }

    public void enableConnectProxy(List<String> urlsToBeProxied) {
        NativeTestServerJni.get()
                .enableConnectProxy(
                        mEmbeddedTestServerAdapter, urlsToBeProxied.toArray(new String[0]));
    }

    public String getEchoBodyURL() {
        return NativeTestServerJni.get().getEchoBodyURL(mEmbeddedTestServerAdapter);
    }

    public String getEchoHeaderURL(String header) {
        return NativeTestServerJni.get().getEchoHeaderURL(mEmbeddedTestServerAdapter, header);
    }

    public String getEchoAllHeadersURL() {
        return NativeTestServerJni.get().getEchoAllHeadersURL(mEmbeddedTestServerAdapter);
    }

    public String getEchoMethodURL() {
        return NativeTestServerJni.get().getEchoMethodURL(mEmbeddedTestServerAdapter);
    }

    public String getUseEncodingURL(String encoding) {
        return NativeTestServerJni.get().getUseEncodingURL(mEmbeddedTestServerAdapter, encoding);
    }

    public String getRedirectToEchoBody() {
        return NativeTestServerJni.get().getRedirectToEchoBodyURL(mEmbeddedTestServerAdapter);
    }

    public String getFileURL(String filePath) {
        return NativeTestServerJni.get().getFileURL(mEmbeddedTestServerAdapter, filePath);
    }

    // Returns a URL that the server will return an Exabyte of data
    public String getExabyteResponseURL() {
        return NativeTestServerJni.get().getExabyteResponseURL(mEmbeddedTestServerAdapter);
    }

    // The following URLs will make NativeTestServer serve a response based on
    // the contents of the corresponding file and its mock-http-headers file.

    public String getSuccessURL() {
        return getFileURL("/success.txt");
    }

    public String getRedirectURL() {
        return getFileURL("/redirect.html");
    }

    public String getMultiRedirectURL() {
        return getFileURL("/multiredirect.html");
    }

    public String getNotFoundURL() {
        return getFileURL("/notfound.html");
    }

    public String getServerErrorURL() {
        return getFileURL("/server_error.txt");
    }

    public int getPort() {
        return NativeTestServerJni.get().getPort(mEmbeddedTestServerAdapter);
    }

    public String getHostPort() {
        return NativeTestServerJni.get().getHostPort(mEmbeddedTestServerAdapter);
    }

    /** See net::test_server::EmbeddedTestServer::registerRequestHandler(). */
    public void registerRequestHandler(HandleRequestCallback callback) {
        NativeTestServerJni.get().registerRequestHandler(mEmbeddedTestServerAdapter, callback);
    }

    /** Java counterpart of native net::test_server::HttpRequest. */
    public static final class HttpRequest {
        private final String mRelativeUrl;
        private final Map<String, String> mHeaders;
        private final String mMethod;
        private final String mAllHeaders;
        private final String mContent;

        public HttpRequest(
                String relativeUrl,
                Map<String, String> headers,
                String method,
                String allHeaders,
                String content) {
            mRelativeUrl = relativeUrl;
            mHeaders = headers;
            mMethod = method;
            mAllHeaders = allHeaders;
            mContent = content;
        }

        public String getRelativeUrl() {
            return mRelativeUrl;
        }

        public Map<String, String> getHeaders() {
            return mHeaders;
        }

        public String getMethod() {
            return mMethod;
        }

        public String getAllHeaders() {
            return mAllHeaders;
        }

        public String getContent() {
            return mContent;
        }
    }

    /** Java counterpart of native net::test_server::RawHttpResponse. */
    public static final class RawHttpResponse {
        private final String mHeaders;
        private final String mContents;

        private RawHttpResponse(String headers, String contents) {
            mHeaders = Objects.requireNonNull(headers);
            mContents = Objects.requireNonNull(contents);
        }

        public static RawHttpResponse createFromHeaders(List<String> headers) {
            return new RawHttpResponse(
                    /* headers= */ String.join("\r\n", Objects.requireNonNull(headers)),
                    /* contents= */ "");
        }

        public String getHeaders() {
            return mHeaders;
        }

        public String getContents() {
            return mContents;
        }
    }

    /** Java counterpart of native net::test_server::EmbeddedTestServer::HandleRequestCallback. */
    public static interface HandleRequestCallback {
        // Note currently we only support RawHttpResponse. We could add support for more flexible
        // response generation if need be.
        public RawHttpResponse handleRequest(HttpRequest httpRequest);
    }

    // The following indirecting methods are needed because jni_zero doesn't support @CalledByNative
    // on nested classes. See https://crbug.com/422988765.

    @CalledByNative
    private static @JniType("cronet::NativeTestServerRawHttpResponse") RawHttpResponse
            handleRequest(
                    HandleRequestCallback callback,
                    @JniType("cronet::NativeTestServerHttpRequest") HttpRequest httpRequest) {
        return callback.handleRequest(httpRequest);
    }

    @CalledByNative
    private static HttpRequest createHttpRequest(
            @JniType("std::string") String relativeUrl,
            // The type alias is to work around https://crbug.com/422972348.
            @JniType("cronet::NativeTestServerHeaderMap") Map<String, String> headers,
            @JniType("std::string") String method,
            @JniType("std::string") String allHeaders,
            @JniType("std::string") String content) {
        return new HttpRequest(relativeUrl, headers, method, allHeaders, content);
    }

    @CalledByNative
    private static @JniType("std::string") String getRawHttpResponseHeaders(
            RawHttpResponse rawHttpResponse) {
        return rawHttpResponse.getHeaders();
    }

    @CalledByNative
    private static @JniType("std::string") String getRawHttpResponseContents(
            RawHttpResponse rawHttpResponse) {
        return rawHttpResponse.getContents();
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        @JniType("long")
        long create(
                @JniType("std::string") String filePath,
                @JniType("std::string") String testDataDir,
                @JniType("bool") boolean useHttps,
                @JniType("net::EmbeddedTestServer::ServerCertificate") @ServerCertificate
                        int certificate);

        void destroy(long nativeEmbeddedTestServerAdapter);

        void start(long nativeEmbeddedTestServerAdapter);

        void enableConnectProxy(
                long nativeEmbeddedTestServerAdapter,
                @JniType("std::vector<std::string>") String[] urls);

        @JniType("std::string")
        String getEchoBodyURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getEchoHeaderURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String header);

        @JniType("std::string")
        String getEchoAllHeadersURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getEchoMethodURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getUseEncodingURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String encoding);

        @JniType("std::string")
        String getRedirectToEchoBodyURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getFileURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String filePath);

        @JniType("std::string")
        String getExabyteResponseURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getHostPort(long nativeEmbeddedTestServerAdapter);

        @JniType("int")
        int getPort(long nativeEmbeddedTestServerAdapter);

        void registerRequestHandler(
                long nativeEmbeddedTestServerAdapter,
                @JniType("std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>")
                        HandleRequestCallback callback);
    }
}
