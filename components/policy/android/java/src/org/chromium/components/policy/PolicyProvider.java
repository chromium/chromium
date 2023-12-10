// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

/** Base class for Policy providers. */
public abstract class PolicyProvider {
    private static final String TAG = "PolicyProvider";

    private CombinedPolicyProvider mCombinedPolicyProvider;
    private int mSource = -1;

    protected PolicyProvider() {}

    public void notifySettingsAvailable(Bundle settings) {
        ThreadUtils.assertOnUiThread();
        Log.i(TAG, "#notifySettingsAvailable() " + mSource);
        mCombinedPolicyProvider.onSettingsAvailable(mSource, settings);
    }

    protected void terminateIncognitoSession() {
        mCombinedPolicyProvider.terminateIncognitoSession();
    }

    /**
     * Called to request a refreshed set of policies. This method must handle notifying the
     * CombinedPolicyProvider whenever applicable.
     */
    public abstract void refresh();

    /** Register the PolicyProvider for receiving policy changes. */
    protected void startListeningForPolicyChanges() {}

    /**
     * Called by the {@link CombinedPolicyProvider} to correctly hook it with the Policy system.
     *
     * @param combinedPolicyProvider reference to the CombinedPolicyProvider to be used like a
     *            delegate.
     * @param source tags the PolicyProvider with a source.
     */
    final void setManagerAndSource(CombinedPolicyProvider combinedPolicyProvider, int source) {
        assert mSource < 0;
        assert source >= 0;
        mSource = source;
        assert mCombinedPolicyProvider == null;
        mCombinedPolicyProvider = combinedPolicyProvider;
        startListeningForPolicyChanges();
        Log.i(TAG, "#setManagerAndSource() " + mSource);
    }

    /** Called when the provider is unregistered */
    public void destroy() {}
}
