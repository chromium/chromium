// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;

import org.chromium.base.Log;
import org.chromium.net.test.util.CertTestUtil;

import java.io.File;

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

/**
 * Wrapper class to start a HTTP/2 test server.
 */
public final class Http2TestServer {
    private static Channel sServerChannel;
    private static final String TAG = Http2TestServer.class.getSimpleName();

    private static final String HOST = "127.0.0.1";
    // Server port.
    private static final int PORT = 8443;

    private static ReportingCollector sReportingCollector;

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
     * @return url of the server resource which will echo every received stream data frame.
     */
    public static String getEchoStreamUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_STREAM_PATH;
    }

    /**
     * @return url of the server resource which will echo request headers as response trailers.
     */
    public static String getEchoTrailersUrl() {
        return getServerUrl() + Http2TestHandler.ECHO_TRAILERS_PATH;
    }

    /**
     * @return url of a brotli-encoded server resource.
     */
    public static String getServeSimpleBrotliResponse() {
        return getServerUrl() + Http2TestHandler.SERVE_SIMPLE_BROTLI_RESPONSE;
    }

    /**
     * @return url of the reporting collector
     */
    public static String getReportingCollectorUrl() {
        return getServerUrl() + Http2TestHandler.REPORTING_COLLECTOR_PATH;
    }

    /**
     * @return url of a resource that includes Reporting and NEL policy headers in its response
     */
    public static String getSuccessWithNELHeadersUrl() {
        return getServerUrl() + Http2TestHandler.SUCCESS_WITH_NEL_HEADERS_PATH;
    }

    public static boolean startHttp2TestServer(
            Context context, String certFileName, String keyFileName) throws Exception {
        sReportingCollector = new ReportingCollector();
        Http2TestServerRunnable http2TestServerRunnable =
                new Http2TestServerRunnable(new File(CertTestUtil.CERTS_DIRECTORY + certFileName),
                        new File(CertTestUtil.CERTS_DIRECTORY + keyFileName));
        new Thread(http2TestServerRunnable).start();
        http2TestServerRunnable.blockUntilStarted();
        return true;
    }

    private Http2TestServer() {}

    private static class Http2TestServerRunnable implements Runnable {
        private final ConditionVariable mBlock = new ConditionVariable();
        private final SslContext mSslCtx;

        Http2TestServerRunnable(File certFile, File keyFile) throws Exception {
            ApplicationProtocolConfig applicationProtocolConfig = new ApplicationProtocolConfig(
                    Protocol.ALPN, SelectorFailureBehavior.NO_ADVERTISE,
                    SelectedListenerFailureBehavior.ACCEPT, ApplicationProtocolNames.HTTP_2);

            // Don't make netty use java.security.KeyStore.getInstance("JKS") as it doesn't
            // exist.  Just avoid a KeyManagerFactory as it's unnecessary for our testing.
            System.setProperty("io.netty.handler.ssl.openssl.useKeyManagerFactory", "false");

            mSslCtx = new OpenSslServerContext(certFile, keyFile, null, null,
                    Http2SecurityUtil.CIPHERS, SupportedCipherSuiteFilter.INSTANCE,
                    applicationProtocolConfig, 0, 0);
        }

        public void blockUntilStarted() {
            mBlock.block();
        }

        @Override
        public void run() {
            try {
                // Configure the server.
                EventLoopGroup group = new NioEventLoopGroup();
                try {
                    ServerBootstrap b = new ServerBootstrap();
                    b.option(ChannelOption.SO_BACKLOG, 1024);
                    b.group(group)
                            .channel(NioServerSocketChannel.class)
                            .handler(new LoggingHandler(LogLevel.INFO))
                            .childHandler(new Http2ServerInitializer(mSslCtx));

                    sServerChannel = b.bind(PORT).sync().channel();
                    Log.i(TAG, "Netty HTTP/2 server started on " + getServerUrl());
                    mBlock.open();
                    sServerChannel.closeFuture().sync();
                } finally {
                    group.shutdownGracefully();
                }
                Log.i(TAG, "Stopped Http2TestServerRunnable!");
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        }
    }

    /**
     * Sets up the Netty pipeline for the test server.
     */
    private static class Http2ServerInitializer extends ChannelInitializer<SocketChannel> {
        private final SslContext mSslCtx;

        public Http2ServerInitializer(SslContext sslCtx) {
            this.mSslCtx = sslCtx;
        }

        @Override
        public void initChannel(SocketChannel ch) {
            ch.pipeline().addLast(mSslCtx.newHandler(ch.alloc()), new Http2NegotiationHandler());
        }
    }

    private static class Http2NegotiationHandler extends ApplicationProtocolNegotiationHandler {
        protected Http2NegotiationHandler() {
            super(ApplicationProtocolNames.HTTP_1_1);
        }

        @Override
        protected void configurePipeline(ChannelHandlerContext ctx, String protocol)
                throws Exception {
            if (ApplicationProtocolNames.HTTP_2.equals(protocol)) {
                ctx.pipeline().addLast(new Http2TestHandler.Builder()
                                               .setReportingCollector(sReportingCollector)
                                               .setServerUrl(getServerUrl())
                                               .build());
                return;
            }

            throw new IllegalStateException("unknown protocol: " + protocol);
        }
    }
}
