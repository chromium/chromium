// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

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
    // NETWORK_CONTEXT_KEY is the key under which a pointer to a NetworkContext
    // is passed (as a long) in the arguments {@link Bundle} to the {@link
    // Fragment} in the module.
    private static final String NETWORK_CONTEXT_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String INSTANCE_ID_DRIVER_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.InstanceIDDriver";
    private static final String SETTINGS_ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.modules.cablev2_authenticator.SettingsActivityClassName";
    private static final String WRAPPER_CLASS_NAME =
            "org.chromium.chrome.modules.cablev2_authenticator.WrapperClassName";
    private TextView mStatus;

    @Override
    @SuppressLint("SetTextI18n")
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final Context context = getContext();

        // This UI is a placeholder for development, has not been reviewed by
        // UX, and thus just uses untranslated strings for now.
        getActivity().setTitle("Installing");

        mStatus = new TextView(context);
        mStatus.setPadding(0, 60, 0, 60);

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.addView(mStatus,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        if (Cablev2AuthenticatorModule.isInstalled()) {
            showModule();
        } else {
            mStatus.setText("Installing security key functionality…");
            Cablev2AuthenticatorModule.install((success) -> {
                if (!success) {
                    mStatus.setText("Failed to install.");
                    return;
                }
                showModule();
            });
        }

        return layout;
    }

    @SuppressLint("SetTextI18n")
    private void showModule() {
        mStatus.setText("Installed.");

        FragmentTransaction transaction =
                getActivity().getSupportFragmentManager().beginTransaction();
        Fragment fragment = Cablev2AuthenticatorModule.getImpl().getFragment();
        Bundle arguments = getArguments();
        if (arguments == null) {
            arguments = new Bundle();
        }
        arguments.putLong(NETWORK_CONTEXT_KEY,
                CableAuthenticatorModuleProviderJni.get().getSystemNetworkContext());
        arguments.putLong(INSTANCE_ID_DRIVER_KEY,
                CableAuthenticatorModuleProviderJni.get().getInstanceIDDriver());
        // SettingsActivity has to be named as a string here because it cannot
        // be depended upon without creating a cycle in the deps graph. It's
        // used as the target of a notification and, in time we'll need our own
        // top-level Activity in order to handle USB devices. For now, though,
        // it serves for testing.
        // TODO(agl): replace with custom top-level Activity.
        arguments.putString(SETTINGS_ACTIVITY_CLASS_NAME,
                "org.chromium.chrome.browser.settings.SettingsActivity");
        arguments.putString(
                WRAPPER_CLASS_NAME, CableAuthenticatorModuleProvider.class.getCanonicalName());
        fragment.setArguments(arguments);
        transaction.replace(getId(), fragment);
        // This fragment is deliberately not added to the back-stack here so
        // that it appears to have been "replaced" by the authenticator UI.
        transaction.commit();
    }

    @NativeMethods
    interface Natives {
        // getSystemNetworkContext returns a pointer, encoded in a long, to the
        // global NetworkContext for system services that hangs off
        // |g_browser|. This is needed because //chrome/browser, being a
        // static_library, cannot be depended on by another component thus we
        // pass this value into the feature module.
        long getSystemNetworkContext();
        long getInstanceIDDriver();
    }
}
