// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import android.annotation.SuppressLint;
import android.os.Build;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalBidirectionalStream;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Map;
import java.util.concurrent.Executor;

/**
 * Implementation of {@link ExperimentalBidirectionalStream.Builder}.
 */
public class BidirectionalStreamBuilderImpl extends ExperimentalBidirectionalStream.Builder {
    // All fields are temporary storage of ExperimentalBidirectionalStream configuration to be
    // copied to CronetBidirectionalStream.

    // CronetEngine to create the stream.
    private final CronetEngineBase mCronetEngine;
    // URL to request.
    private final String mUrl;
    // Callback to receive progress callbacks.
    private final BidirectionalStream.Callback mCallback;
    // Executor on which callbacks will be invoked.
    private final Executor mExecutor;
    // List of request headers, stored as header field name and value pairs.
    private final ArrayList<Map.Entry<String, String>> mRequestHeaders = new ArrayList<>();

    // HTTP method for the request. Default to POST.
    private String mHttpMethod = "POST";
    // Priority of the stream. Default is medium.
    @CronetEngineBase.StreamPriority
    private int mPriority = STREAM_PRIORITY_MEDIUM;

    private boolean mDelayRequestHeadersUntilFirstFlush;

    // Request reporting annotations.
    private Collection<Object> mRequestAnnotations;

    private boolean mTrafficStatsTagSet;
    private int mTrafficStatsTag;
    private boolean mTrafficStatsUidSet;
    private int mTrafficStatsUid;
    private long mNetworkHandle = CronetEngineBase.DEFAULT_NETWORK_HANDLE;

    /**
     * Creates a builder for {@link BidirectionalStream} objects. All callbacks for
     * generated {@code BidirectionalStream} objects will be invoked on
     * {@code executor}. {@code executor} must not run tasks on the
     * current thread, otherwise the networking operations may block and exceptions
     * may be thrown at shutdown time.
     *
     * @param url the URL for the generated stream
     * @param callback the {@link BidirectionalStream.Callback} object that gets invoked upon
     * different events
     *     occuring
     * @param executor the {@link Executor} on which {@code callback} methods will be invoked
     * @param cronetEngine the {@link CronetEngine} used to create the stream
     */
    BidirectionalStreamBuilderImpl(String url, BidirectionalStream.Callback callback,
            Executor executor, CronetEngineBase cronetEngine) {
        super();
        if (url == null) {
            throw new NullPointerException("URL is required.");
        }
        if (callback == null) {
            throw new NullPointerException("Callback is required.");
        }
        if (executor == null) {
            throw new NullPointerException("Executor is required.");
        }
        if (cronetEngine == null) {
            throw new NullPointerException("CronetEngine is required.");
        }
        mUrl = url;
        mCallback = callback;
        mExecutor = executor;
        mCronetEngine = cronetEngine;
    }

    @Override
    public BidirectionalStreamBuilderImpl setHttpMethod(String method) {
        if (method == null) {
            throw new NullPointerException("Method is required.");
        }
        mHttpMethod = method;
        return this;
    }

    @Override
    public BidirectionalStreamBuilderImpl addHeader(String header, String value) {
        if (header == null) {
            throw new NullPointerException("Invalid header name.");
        }
        if (value == null) {
            throw new NullPointerException("Invalid header value.");
        }
        mRequestHeaders.add(new AbstractMap.SimpleImmutableEntry<>(header, value));
        return this;
    }

    @Override
    public BidirectionalStreamBuilderImpl setPriority(
            @CronetEngineBase.StreamPriority int priority) {
        mPriority = priority;
        return this;
    }

    @Override
    public BidirectionalStreamBuilderImpl delayRequestHeadersUntilFirstFlush(
            boolean delayRequestHeadersUntilFirstFlush) {
        mDelayRequestHeadersUntilFirstFlush = delayRequestHeadersUntilFirstFlush;
        return this;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder addRequestAnnotation(Object annotation) {
        if (annotation == null) {
            throw new NullPointerException("Invalid metrics annotation.");
        }
        if (mRequestAnnotations == null) {
            mRequestAnnotations = new ArrayList<Object>();
        }
        mRequestAnnotations.add(annotation);
        return this;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder setTrafficStatsTag(int tag) {
        mTrafficStatsTagSet = true;
        mTrafficStatsTag = tag;
        return this;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder setTrafficStatsUid(int uid) {
        mTrafficStatsUidSet = true;
        mTrafficStatsUid = uid;
        return this;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder bindToNetwork(long networkHandle) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            throw new UnsupportedOperationException(
                    "The multi-network API is available starting from Android Marshmallow");
        }
        mNetworkHandle = networkHandle;
        return this;
    }

    @Override
    @SuppressLint("WrongConstant") // TODO(jbudorick): Remove this after rolling to the N SDK.
    public ExperimentalBidirectionalStream build() {
        return mCronetEngine.createBidirectionalStream(mUrl, mCallback, mExecutor, mHttpMethod,
                mRequestHeaders, mPriority, mDelayRequestHeadersUntilFirstFlush,
                mRequestAnnotations, mTrafficStatsTagSet, mTrafficStatsTag, mTrafficStatsUidSet,
                mTrafficStatsUid, mNetworkHandle);
    }
}
