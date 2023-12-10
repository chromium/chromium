// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static io.netty.handler.codec.http.HttpResponseStatus.OK;
import static io.netty.handler.codec.http.HttpVersion.HTTP_1_1;

import android.os.ConditionVariable;
import android.util.Log;

import io.netty.bootstrap.ServerBootstrap;
import io.netty.buffer.Unpooled;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelPipeline;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.http.DefaultFullHttpResponse;
import io.netty.handler.codec.http.FullHttpResponse;
import io.netty.handler.codec.http.HttpRequestDecoder;
import io.netty.handler.codec.http.HttpResponseEncoder;
import io.netty.handler.logging.LogLevel;
import io.netty.handler.logging.LoggingHandler;
import io.netty.util.CharsetUtil;

/** A simple HTTP server for testing. */
public class HttpTestServer implements TestSupport.TestServer {
    private static final String TAG = HttpTestServer.class.getSimpleName();
    private static final String HOST = "127.0.0.1";
    private static final int PORT = 8080;

    private Channel mServerChannel;
    private ConditionVariable mStartBlock = new ConditionVariable();
    private ConditionVariable mShutdownBlock = new ConditionVariable();

    @Override
    public boolean start() {
        new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    HttpTestServer.this.run();
                                } catch (Exception e) {
                                    Log.e(TAG, "Unable to start HttpTestServer", e);
                                }
                            }
                        })
                .start();
        // Return false if the server cannot start within 5 seconds.
        return mStartBlock.block(5000);
    }

    @Override
    public void shutdown() {
        if (mServerChannel != null) {
            mServerChannel.close();
            boolean success = mShutdownBlock.block(10000);
            if (!success) {
                Log.e(TAG, "Unable to shutdown the server. Is it already dead?");
            }
            mServerChannel = null;
        }
    }

    @Override
    public String getSuccessURL() {
        return getServerUrl() + "/success";
    }

    private String getServerUrl() {
        return "http://" + HOST + ":" + PORT;
    }

    private void run() throws Exception {
        EventLoopGroup bossGroup = new NioEventLoopGroup(1);
        EventLoopGroup workerGroup = new NioEventLoopGroup(4);
        try {
            ServerBootstrap b = new ServerBootstrap();
            b.group(bossGroup, workerGroup)
                    .channel(NioServerSocketChannel.class)
                    .handler(new LoggingHandler(LogLevel.INFO))
                    .childHandler(
                            new ChannelInitializer<SocketChannel>() {
                                @Override
                                public void initChannel(SocketChannel ch) throws Exception {
                                    ChannelPipeline p = ch.pipeline();
                                    p.addLast(new HttpRequestDecoder());
                                    p.addLast(new HttpResponseEncoder());
                                    p.addLast(new TestServerHandler());
                                }
                            });

            // Start listening fo incoming connections.
            mServerChannel = b.bind(PORT).sync().channel();
            mStartBlock.open();
            // Block until the channel is closed.
            mServerChannel.closeFuture().sync();
            mShutdownBlock.open();
            Log.i(TAG, "HttpServer stopped");
        } finally {
            workerGroup.shutdownGracefully();
            bossGroup.shutdownGracefully();
        }
    }

    private static class TestServerHandler extends SimpleChannelInboundHandler {
        @Override
        protected void channelRead0(ChannelHandlerContext ctx, Object msg) throws Exception {
            FullHttpResponse response =
                    new DefaultFullHttpResponse(
                            HTTP_1_1, OK, Unpooled.copiedBuffer("Hello!", CharsetUtil.UTF_8));
            ctx.writeAndFlush(response).addListener(ChannelFutureListener.CLOSE);
        }
    }
}
