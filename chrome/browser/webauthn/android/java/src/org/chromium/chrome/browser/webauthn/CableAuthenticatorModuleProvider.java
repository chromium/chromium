// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.modules.cablev2_authenticator.Cablev2AuthenticatorModule;

/**
 * Provides a UI that attempts to install the caBLEv2 Authenticator module. If already installed, or
 * successfully installed, it replaces itself in the back-stack with the authenticator UI.
 *
 * This code lives in the base module, i.e. is _not_ part of the dynamically-loaded module.
 *
 * This does not use {@link ModuleInstallUi} because it needs to integrate into the Fragment-based
 * settings UI, while {@link ModuleInstallUi} assumes that the UI does in a {@link Tab}.
 */
public class CableAuthenticatorModuleProvider extends Fragment {
    // TAG is subject to a 20 character limit.
    private static final String TAG = "CableAuthModuleProv";

    // NETWORK_CONTEXT_KEY is the key under which a pointer to a NetworkContext
    // is passed (as a long) in the arguments {@link Bundle} to the {@link
    // Fragment} in the module.
    private static final String NETWORK_CONTEXT_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String ACTIVITY_CLASS_NAME_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.ActivityClassName";
    private static final String ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.browser.webauth.authenticator.CableAuthenticatorActivity";
    private static final String SECRET_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final Context context = getContext();

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);

        if (Cablev2AuthenticatorModule.isInstalled()) {
            showModule();
        } else {
            Cablev2AuthenticatorModule.install((success) -> {
                if (!success) {
                    Log.e(TAG, "Failed to install caBLE DFM");
                    getActivity().finish();
                    return;
                }
                showModule();
            });
        }

        return layout;
    }

    private void showModule() {
        FragmentTransaction transaction = getParentFragmentManager().beginTransaction();
        Fragment fragment = Cablev2AuthenticatorModule.getImpl().getFragment();
        Bundle arguments = getArguments();
        if (arguments == null) {
            arguments = new Bundle();
        }
        arguments.putLong(NETWORK_CONTEXT_KEY,
                CableAuthenticatorModuleProviderJni.get().getSystemNetworkContext());
        arguments.putLong(
                REGISTRATION_KEY, CableAuthenticatorModuleProviderJni.get().getRegistration());
        arguments.putString(ACTIVITY_CLASS_NAME_KEY, ACTIVITY_CLASS_NAME);
        arguments.putByteArray(SECRET_KEY, CableAuthenticatorModuleProviderJni.get().getSecret());
        fragment.setArguments(arguments);
        transaction.replace(getId(), fragment);
        // This fragment is deliberately not added to the back-stack here so
        // that it appears to have been "replaced" by the authenticator UI.
        transaction.commit();
    }

    /**
     * onCloudMessage is called by native code when a GCM message is received.
     *
     * @param event a pointer to a |device::cablev2::authenticator::Registration::Event| which this
     *         code takes ownership of.
     */
    @CalledByNative
    public static void onCloudMessage(long event) {
        final long networkContext =
                CableAuthenticatorModuleProviderJni.get().getSystemNetworkContext();
        final long registration = CableAuthenticatorModuleProviderJni.get().getRegistration();
        final byte[] secret = CableAuthenticatorModuleProviderJni.get().getSecret();

        if (Cablev2AuthenticatorModule.isInstalled()) {
            Cablev2AuthenticatorModule.getImpl().onCloudMessage(
                    event, networkContext, registration, ACTIVITY_CLASS_NAME, secret);
            return;
        }

        Cablev2AuthenticatorModule.install((success) -> {
            if (!success) {
                CableAuthenticatorModuleProviderJni.get().freeEvent(event);
                return;
            }
            Cablev2AuthenticatorModule.getImpl().onCloudMessage(
                    event, networkContext, registration, ACTIVITY_CLASS_NAME, secret);
        });
    }

    @NativeMethods
    interface Natives {
        // getSystemNetworkContext returns a pointer, encoded in a long, to the
        // global NetworkContext for system services that hangs off
        // |g_browser|. This is needed because //chrome/browser, being a
        // static_library, cannot be depended on by another component thus we
        // pass this value into the feature module.
        long getSystemNetworkContext();
        // getRegistration returns a pointer to the global
        // device::cablev2::authenticator::Registration.
        long getRegistration();
        // getSecret returns a 32-byte secret from which can be derived the
        // key and shared secret that were advertised via Sync.
        byte[] getSecret();
        // freeEvent releases resources used by the given event.
        void freeEvent(long event);
    }
}
