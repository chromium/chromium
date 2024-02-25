// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModelProvider;

import org.chromium.base.Log;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class MainFragment extends Fragment {
    private static final String TAG = "CronetSampleApp";
    private EditText mUrlEditText;
    private TextView mResultText;
    private Button mStartButton;
    private Button mResetEngineButton;
    private Button mClearTextButton;
    private SampleActivityViewModel mActivityViewModel;

    private CronetEngine getCronetEngine() {
        return ((CronetSampleApplication) requireActivity().getApplication()).getCronetEngine();
    }

    private void resetEngine() {
        ((CronetSampleApplication) requireActivity().getApplication()).restartCronetEngine();
    }

    private void init(View view) {
        mUrlEditText = view.findViewById(R.id.url_edittext);
        mResultText = view.findViewById(R.id.result_textview);
        mStartButton = view.findViewById(R.id.start_button);
        mResetEngineButton = view.findViewById(R.id.reset_button);
        mClearTextButton = view.findViewById(R.id.clear_button);
        mStartButton.setOnClickListener(
                v -> {
                    Executor executor = Executors.newSingleThreadExecutor();
                    UrlRequest.Callback callback = new SimpleUrlRequestCallback();
                    UrlRequest.Builder builder =
                            getCronetEngine()
                                    .newUrlRequestBuilder(
                                            mUrlEditText.getText().toString(), callback, executor);
                    builder.build().start();
                });

        mResetEngineButton.setOnClickListener(v -> resetEngine());
        mClearTextButton.setOnClickListener(v -> mResultText.setText(""));
        mActivityViewModel =
                new ViewModelProvider((FragmentActivity) requireActivity())
                        .get(SampleActivityViewModel.class);
    }

    @Nullable
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.main_fragment, container, false);
        init(view);
        return view;
    }

    class SimpleUrlRequestCallback extends UrlRequest.Callback {
        private ByteArrayOutputStream mBytesReceived = new ByteArrayOutputStream();
        private WritableByteChannel mReceiveChannel = Channels.newChannel(mBytesReceived);

        @Override
        public void onRedirectReceived(
                UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
            Log.i(TAG, "****** onRedirectReceived ******");
            request.followRedirect();
        }

        @Override
        public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
            Log.i(TAG, "****** Response Started ******");
            Log.i(TAG, "*** Headers Are *** " + info.getAllHeaders());
            if (Options.isBooleanOptionOn(Options.OptionsIdentifier.SLOW_DOWNLOAD)) {
                try {
                    Thread.sleep(10000);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException(e);
                }
            }
            request.read(ByteBuffer.allocateDirect(32 * 1024));
        }

        @Override
        public void onReadCompleted(
                UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
            byteBuffer.flip();
            Log.i(TAG, "****** onReadCompleted ******" + byteBuffer);

            try {
                mReceiveChannel.write(byteBuffer);
            } catch (IOException e) {
                Log.i(TAG, "IOException during ByteBuffer read. Details: ", e);
            }
            byteBuffer.clear();
            request.read(byteBuffer);
            if (Options.isBooleanOptionOn(Options.OptionsIdentifier.SLOW_DOWNLOAD)) {
                try {
                    Thread.sleep(10000);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException(e);
                }
            }
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            Log.i(
                    TAG,
                    "****** Request Completed, status code is "
                            + info.getHttpStatusCode()
                            + ", total received bytes is "
                            + info.getReceivedByteCount());
            final String receivedData = mBytesReceived.toString();
            final String url = info.getUrl();
            final String text = "Completed " + url + " (" + info.getHttpStatusCode() + ")";
            new Handler(Looper.getMainLooper())
                    .post(() -> mResultText.setText(String.format("%s\n%s", text, receivedData)));
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
            Log.i(TAG, "****** onFailed, error is: " + error.getMessage());
            final String text = "Failed " + " (" + error.getMessage() + ")";
            new Handler(Looper.getMainLooper())
                    .post(() -> mResultText.setText(String.format("%s", text)));
        }
    }

    // Starts writing NetLog to disk. startNetLog() should be called afterwards.
    private void startNetLog() {
        getCronetEngine()
                .startNetLogToFile(
                        requireActivity().getCacheDir().getPath() + "/netlog.json", false);
    }

    // Stops writing NetLog to disk. Should be called after calling startNetLog().
    // NetLog can be downloaded afterwards via:
    //   adb root
    //   adb pull /data/data/org.chromium.cronet_sample_apk/cache/netlog.json
    // netlog.json can then be viewed in a Chrome tab navigated to chrome://net-internals/#import
    private void stopNetLog() {
        getCronetEngine().stopNetLog();
    }
}
