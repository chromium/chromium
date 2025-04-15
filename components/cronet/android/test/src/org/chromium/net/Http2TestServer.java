// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;

import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.Channel;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.http2.Http2SecurityUtil;
import io.netty.handler.logging.LogLevel;
import io.netty.handler.logging.LoggingHandler;
import io.netty.handler.ssl.ApplicationProtocolConfig;
import io.netty.handler.ssl.ApplicationProtocolConfig.Protocol;
import io.netty.handler.ssl.ApplicationProtocolConfig.SelectedListenerFailureBehavior;
import io.netty.handler.ssl.ApplicationProtocolConfig.SelectorFailureBehavior;
import io.netty.handler.ssl.ApplicationProtocolNames;
import io.netty.handler.ssl.ApplicationProtocolNegotiationHandler;
import io.netty.handler.ssl.OpenSslServerContext;
import io.netty.handler.ssl.SslContext;
import io.netty.handler.ssl.SupportedCipherSuiteFilter;

import org.chromium.base.Log;
import org.chromium.net.test.util.CertTestUtil;

import java.io.File;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Wrapper class to start a HTTP/2 test server. */
public final class Http2TestServer {
    private static Channel sServerChannel;
    private static final String TAG = Http2TestServer.class.getSimpleName();

    private static final String HOST = "localhost";
    // Server port.
    private static final int PORT = 8443;

    private static ReportingCollector sReportingCollector;

    public static final String SERVER_CERT_PEM;
    private static final String SERVER_KEY_PKCS8_PEM;
    // Used to start http2 test server.
    private static final ExecutorService EXECUTOR = Executors.newFixedThreadPool(1);

    static {
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        // Currently, MockCertVerifier uses different certificates, so make the server also use
        // those.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            SERVER_CERT_PEM = "quic-chain.pem";
            SERVER_KEY_PKCS8_PEM = "quic-leaf-cert.key.pkcs8.pem";
        } else {
            SERVER_CERT_PEM = "cronet-quic-chain.pem";
            SERVER_KEY_PKCS8_PEM = "cronet-quic-leaf-cert.key.pkcs8.pem";
        }
    }

    public static boolean shutdownHttp2TestServer() throws Exception {
        if (sServerChannel != null) {
            sServerChannel.close().sync();
            sServerChannel = null;
            sReportingCollector = null;
            return true;
        }
        return false;
    }

    public static String getServerHost() {
        return HOST;
    }

    public static int getServerPort() {
        return PORT;
    }

    public static String getServerUrl() {
        return "https://" + HOST + ":" + PORT;
    }

    public static ReportingCollector getReportingCollector() {
        return sReportingCollector;
    }

    public static String getEchoAllHeadersUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_ALL_HEADERS_PATH;
    }

    public static String getEchoHeaderUrl(String headerName) {
        return getServerUrl() + Http2TestHandler.ECHO_HEADER_PATH + "?" + headerName;
    }

    public static String getEchoMethodUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_METHOD_PATH;
    }

    /**
     * When using this you must provide a CountDownLatch in the call to startHttp2TestServer.
     * The request handler will continue to hang until the provided CountDownLatch reaches 0.
     *
     * @return url of the server resource which will hang indefinitely.
     */
    public static String getHangingRequestUrl() {
        return getServerUrl() + Http2TestHandler.HANGING_REQUEST_PATH;
    }

    /** @return url of the server resource which will echo every received stream data frame. */
    public static String getEchoStreamUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_STREAM_PATH;
    }

    /** @return url of the server resource which will echo request headers as response trailers. */
    public static String getEchoTrailersUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_TRAILERS_PATH;
    }

    /** @return url of a brotli-encoded server resource. */
    public static String getServeSimpleBrotliResponse() {
        return getServerUrl() + Http2TestHandler.SERVE_SIMPLE_BROTLI_RESPONSE;
    }

    /**
     * @return url of a shared-brotli-encoded server resource.
     */
    public static String getServeSharedBrotliResponse() {
        return getServerUrl() + Http2TestHandler.SERVE_SHARED_BROTLI_RESPONSE;
    }

    /**
     * @return url of the reporting collector
     */
    public static String getReportingCollectorUrl() {
        return getServerUrl() + Http2TestHandler.REPORTING_COLLECTOR_PATH;
    }

    /** @return url of a resource that includes Reporting and NEL policy headers in its response */
    public static String getSuccessWithNELHeadersUrl() {
        return getServerUrl() + Http2TestHandler.SUCCESS_WITH_NEL_HEADERS_PATH;
    }

    /** @return url of a resource that sends response headers with the same key */
    public static String getCombinedHeadersUrl() {
        return getServerUrl() + Http2TestHandler.COMBINED_HEADERS_PATH;
    }

    public static boolean startHttp2TestServer(Context context) throws Exception {
        TestFilesInstaller.installIfNeeded(context);
        return startHttp2TestServer(context, SERVER_CERT_PEM, SERVER_KEY_PKCS8_PEM, null);
    }

    public static boolean startHttp2TestServer(Context context, CountDownLatch hangingUrlLatch)
            throws Exception {
        TestFilesInstaller.installIfNeeded(context);
        return startHttp2TestServer(
                context, SERVER_CERT_PEM, SERVER_KEY_PKCS8_PEM, hangingUrlLatch);
    }

    private static boolean startHttp2TestServer(
            Context context,
            String certFileName,
            String keyFileName,
            CountDownLatch hangingUrlLatch)
            throws Exception {
        sReportingCollector = new ReportingCollector();
        Http2TestServerRunnable http2TestServerRunnable =
                new Http2TestServerRunnable(
                        new File(CertTestUtil.CERTS_DIRECTORY + certFileName),
                        new File(CertTestUtil.CERTS_DIRECTORY + keyFileName),
                        hangingUrlLatch);
        // This will run synchronously as we can't run the test before we have
        // started the test-server, if the test-server has failed to start then
        // the caller should assert on the value returned to make sure that the test
        // fails if the server has failed to start up.
        return EXECUTOR.submit(http2TestServerRunnable).get();
    }

    private Http2TestServer() {}

    private static class Http2TestServerRunnable implements Callable<Boolean> {
        private final SslContext mSslCtx;
        private final CountDownLatch mHangingUrlLatch;

        Http2TestServerRunnable(File certFile, File keyFile, CountDownLatch hangingUrlLatch)
                throws Exception {
            ApplicationProtocolConfig applicationProtocolConfig =
                    new ApplicationProtocolConfig(
                            Protocol.ALPN, SelectorFailureBehavior.NO_ADVERTISE,
                            SelectedListenerFailureBehavior.ACCEPT,
                                    ApplicationProtocolNames.HTTP_2);

            // Don't make netty use java.security.KeyStore.getInstance("JKS") as it doesn't
            // exist.  Just avoid a KeyManagerFactory as it's unnecessary for our testing.
            System.setProperty("io.netty.handler.ssl.openssl.useKeyManagerFactory", "false");

            mSslCtx =
                    new OpenSslServerContext(
                            certFile,
                            keyFile,
                            null,
                            null,
                            Http2SecurityUtil.CIPHERS,
                            SupportedCipherSuiteFilter.INSTANCE,
                            applicationProtocolConfig,
                            0,
                            0);

            mHangingUrlLatch = hangingUrlLatch;
        }

        @Override
        public Boolean call() throws Exception {
            for(int retries = 0; retries < 10; retries++) {
                try {
                    // Configure the server.
                    EventLoopGroup group = new NioEventLoopGroup();
                    ServerBootstrap b = new ServerBootstrap();
                    b.option(ChannelOption.SO_BACKLOG, 1024);
                    b.group(group)
                            .channel(NioServerSocketChannel.class)
                            .handler(new LoggingHandler(LogLevel.INFO))
                            .childHandler(new Http2ServerInitializer(mSslCtx, mHangingUrlLatch));

                    sServerChannel = b.bind(PORT).sync().channel();
                    Log.i(TAG, "Netty HTTP/2 server started on " + getServerUrl());
                    return true;
                } catch (Exception e) {
                    // Netty test server fails to startup and this is a common issue
                    // https://github.com/netty/netty/issues/2616. It is not well understood
                    // why this is happening or how to fix it, we can workaround this by
                    // trying to restart the server several times before giving up.
                    // See crbug/1519471 for more information.
                    Log.w(TAG, "Netty server failed to start", e);
                    // Sleep for half a second before trying again.
                    Thread.sleep(/* milliseconds = */ 500);
                }
            }
            return false;
        }
    }

    /** Sets up the Netty pipeline for the test server. */
    private static class Http2ServerInitializer extends ChannelInitializer<SocketChannel> {
        private final SslContext mSslCtx;
        private final CountDownLatch mHangingUrlLatch;

        public Http2ServerInitializer(SslContext sslCtx, CountDownLatch hangingUrlLatch) {
            mSslCtx = sslCtx;
            mHangingUrlLatch = hangingUrlLatch;
        }

        @Override
        public void initChannel(SocketChannel ch) {
            ch.pipeline()
                    .addLast(
                            mSslCtx.newHandler(ch.alloc()),
                            new Http2NegotiationHandler(mHangingUrlLatch));
        }
    }

    private static class Http2NegotiationHandler extends ApplicationProtocolNegotiationHandler {
        private final CountDownLatch mHangingUrlLatch;

        protected Http2NegotiationHandler(CountDownLatch hangingUrlLatch) {
            super(ApplicationProtocolNames.HTTP_1_1);
            mHangingUrlLatch = hangingUrlLatch;
        }

        @Override
        protected void configurePipeline(ChannelHandlerContext ctx, String protocol)
                throws Exception {
            if (ApplicationProtocolNames.HTTP_2.equals(protocol)) {
                ctx.pipeline()
                        .addLast(
                                new Http2TestHandler.Builder()
                                        .setReportingCollector(sReportingCollector)
                                        .setServerUrl(getServerUrl())
                                        .setHangingUrlLatch(mHangingUrlLatch)
                                        .build());
                return;
            }

            throw new IllegalStateException("unknown protocol: " + protocol);
        }
    }
}
