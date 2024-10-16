// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.app.KeyguardManager;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.os.Parcel;

import androidx.core.app.NotificationManagerCompat;

import com.google.android.gms.tasks.Task;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.webauthn.Fido2ApiCall;

/**
 * Provides linking information to the native side.
 *
 * <p>TODO(crbug.com/348204152): Rename this class to CableInformationProvider and consider
 * providing the information from GMSCore.
 */
public class CableAuthenticatorModuleProvider {
    // TAG is subject to a 20 character limit.
    private static final String TAG = "CableAuthModuleProv";

    /**
     * onCloudMessage is called by native code when a GCM message is received.
     *
     * @param event a pointer to a |device::cablev2::authenticator::Registration::Event| which this
     *     code takes ownership of.
     */
    @CalledByNative
    public static void onCloudMessage(byte[] serializedEvent, boolean isMakeCredential) {
        // TODO(crbug.com/348204152): Remove the native parts.
    }

    @CalledByNative
    public static boolean canDeviceSupportCable() {
        // This function will be run on a background thread.

        if (BluetoothAdapter.getDefaultAdapter() == null) {
            return false;
        }

        // GMSCore will immediately fail all requests if a screenlock
        // isn't configured.
        final Context context = ContextUtils.getApplicationContext();
        KeyguardManager km = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        if (!km.isDeviceSecure()) {
            return false;
        }

        return NotificationManagerCompat.from(context).areNotificationsEnabled();
    }

    /** Calls back into native code with whether we are running in a work profile. */
    @CalledByNative
    public static void amInWorkProfile(long pointer) {
        ThreadUtils.assertOnUiThread();
        EnterpriseInfo enterpriseInfo = EnterpriseInfo.getInstance();
        enterpriseInfo.getDeviceEnterpriseInfo(
                (state) ->
                        CableAuthenticatorModuleProviderJni.get()
                                .onHaveWorkProfileResult(pointer, state.mProfileOwned));
    }

    @CalledByNative
    public static void getLinkingInformation(long pointer) {
        boolean ok = true;
        if (!ExternalAuthUtils.getInstance().canUseFirstPartyGooglePlayServices()) {
            Log.i(TAG, "Cannot get linking information from Play Services without 1p access.");
            ok = false;
        } else if (PackageUtils.getPackageVersion("com.google.android.gms") < 232400000) {
            Log.i(TAG, "GMS Core version is too old to get linking information.");
            ok = false;
        }

        if (!ok) {
            CableAuthenticatorModuleProviderJni.get().onHaveLinkingInformation(pointer, null);
            return;
        }

        Fido2ApiCall call =
                new Fido2ApiCall(
                        ContextUtils.getApplicationContext(), Fido2ApiCall.FIRST_PARTY_API);
        Parcel args = call.start();
        Fido2ApiCall.ByteArrayResult result = new Fido2ApiCall.ByteArrayResult();
        args.writeStrongBinder(result);
        Task<byte[]> task =
                call.run(
                        Fido2ApiCall.METHOD_GET_LINK_INFO,
                        Fido2ApiCall.TRANSACTION_GET_LINK_INFO,
                        args,
                        result);
        task.addOnSuccessListener(
                        linkInfo -> {
                            CableAuthenticatorModuleProviderJni.get()
                                    .onHaveLinkingInformation(pointer, linkInfo);
                        })
                .addOnFailureListener(
                        exception -> {
                            Log.e(
                                    TAG,
                                    "Call to get linking information from Play Services failed",
                                    exception);
                            CableAuthenticatorModuleProviderJni.get()
                                    .onHaveLinkingInformation(pointer, null);
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

        // onHaveLinkingInformation is called when pre-link information has been received from Play
        // Services. The argument is a CBOR-encoded linking structure, as defined in CTAP 2.2, or is
        // null on error.
        void onHaveLinkingInformation(long pointer, byte[] cbor);

        // onHaveWorkProfileResult is called when it has been determined if
        // Chrome is running in a work profile or not.
        void onHaveWorkProfileResult(long pointer, boolean inWorkProfile);
    }
}
