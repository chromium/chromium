// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Fragment;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.base.CastSwitches;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Unit;

/**
 * Fragment for displaying a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. If the CastContentWindowAndroid is destroyed,
 * CastWebContentsFragment should be removed from the activity holding it.
 * Similarily, if the fragment is removed from a activity or the activity holding
 * it is destroyed, CastContentWindowAndroid should be notified by intent.
 *
 * TODO(vincentli): Add a test case to test its lifecycle
 */
public class CastWebContentsFragment extends Fragment {
    private static final String TAG = "CastWebContentFrg";

    private final Controller<Unit> mResumedState = new Controller<>();

    private Context mPackageContext;
    private CastWebContentsSurfaceHelper mSurfaceHelper;
    private View mFragmentRootView;
    private String mAppId;
    private int mInitialVisiblityPriority;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        Log.d(TAG, "onCreateView");
        super.onCreateView(inflater, container, savedInstanceState);

        if (!CastBrowserHelper.initializeBrowser(getContext())) {
            Toast.makeText(getContext(), R.string.browser_process_initialization_failed,
                         Toast.LENGTH_SHORT)
                    .show();
            return null;
        }
        if (mFragmentRootView == null) {
            mFragmentRootView = inflater.cloneInContext(getContext())
                                        .inflate(R.layout.cast_web_contents_activity, null);
        }
        mFragmentRootView.setVisibility(View.VISIBLE);
        return mFragmentRootView;
    }

    @Override
    public Context getContext() {
        if (mPackageContext == null) {
            mPackageContext = ContextUtils.getApplicationContext();
        }
        return mPackageContext;
    }

    @Override
    public void onStart() {
        Log.d(TAG, "onStart");
        super.onStart();

        if (mSurfaceHelper != null) {
            return;
        }

        mSurfaceHelper = new CastWebContentsSurfaceHelper(
                CastWebContentsScopes.onLayoutFragment(getActivity(),
                        (FrameLayout) getView().findViewById(R.id.web_contents_container),
                        CastSwitches.getSwitchValueColor(
                                CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK)),
                (Uri uri) -> sendIntentSync(CastWebContentsIntentUtils.onWebContentStopped(uri)));

        Bundle bundle = getArguments();
        CastWebContentsSurfaceHelper.StartParams params =
                CastWebContentsSurfaceHelper.StartParams.fromBundle(bundle);
        if (params == null) return;

        mAppId = CastWebContentsIntentUtils.getAppId(bundle);
        mInitialVisiblityPriority = CastWebContentsIntentUtils.getVisibilityPriority(bundle);
        mSurfaceHelper.onNewStartParams(params);

        // Whenever our app is visible, volume controls should modify the music stream.
        // For more information read:
        // http://developer.android.com/training/managing-audio/volume-playback.html
        getActivity().setVolumeControlStream(AudioManager.STREAM_MUSIC);
    }

    @Override
    public void onPause() {
        Log.d(TAG, "onPause");
        super.onPause();
        // Set mFragmentRootView to invisible to avoid dismiss fragment animation -> activity back
        // ground -> one frame of cast app UI -> acivity back ground
        mFragmentRootView.setVisibility(View.INVISIBLE);
        mResumedState.reset();
    }

    @Override
    public void onResume() {
        Log.d(TAG, "onResume");
        super.onResume();
        mResumedState.set(Unit.unit());
    }

    private void setToVisible() {
        if (isResumed()) {
            mFragmentRootView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onStop() {
        Log.d(TAG, "onStop");
        super.onStop();
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onDestroy();
        }
        super.onDestroy();
    }

    private void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }
}
