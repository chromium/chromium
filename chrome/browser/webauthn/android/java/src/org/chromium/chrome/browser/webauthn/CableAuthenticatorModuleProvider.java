// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.app.KeyguardManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Parcel;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import com.google.android.gms.tasks.Task;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.modules.cablev2_authenticator.Cablev2AuthenticatorModule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.webauthn.Fido2ApiCall;

/**
 * Provides a UI that attempts to install the caBLEv2 Authenticator module. If already installed, or
 * successfully installed, it replaces itself in the back-stack with the authenticator UI.
 *
 * <p>This code lives in the base module, i.e. is _not_ part of the dynamically-loaded module.
 *
 * <p>This does not use {@link ModuleInstallUi} because it needs to integrate into the
 * Fragment-based settings UI, while {@link ModuleInstallUi} assumes that the UI does in a {@link
 * Tab}.
 */
public class CableAuthenticatorModuleProvider extends Fragment implements OnClickListener {
    // TAG is subject to a 20 character limit.
    private static final String TAG = "CableAuthModuleProv";

    // NOTIFICATION_TIMEOUT_SECS is the number of seconds that a notification
    // will exist for. This stop ignored notifications hanging around.
    private static final int NOTIFICATION_TIMEOUT_SECS = 60;

    // Error codes from the module start at 100, therefore errors outside of the
    // module count downwards so as never to collide.
    private static final int INSTALL_FAILURE_ERROR_CODE = 99;

    // NETWORK_CONTEXT_KEY is the key under which a pointer to a NetworkContext
    // is passed (as a long) in the arguments {@link Bundle} to the {@link
    // Fragment} in the module.
    private static final String NETWORK_CONTEXT_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.browser.webauth.authenticator.CableAuthenticatorActivity";
    private static final String SECRET_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";

    private View mErrorView;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final ViewGroup top = new LinearLayout(getContext());

        // Inflate the error view in case it's needed later.
        mErrorView = inflater.inflate(R.layout.cablev2_error, container, false);
        mErrorView.findViewById(R.id.error_close).setOnClickListener(this);
        ((TextView) mErrorView.findViewById(R.id.error_code))
                .setText(
                        getResources()
                                .getString(
                                        R.string.cablev2_error_code, INSTALL_FAILURE_ERROR_CODE));

        ((TextView) mErrorView.findViewById(R.id.error_description))
                .setText(getResources().getString(R.string.cablev2_error_generic));

        if (Cablev2AuthenticatorModule.isInstalled()) {
            showModule();
        } else {
            Cablev2AuthenticatorModule.install(
                    (success) -> {
                        if (!success) {
                            Log.e(TAG, "Failed to install caBLE DFM");
                            // This can either happen synchronously or asynchronously.
                            // If it happens synchronously then `onCreateView` hasn't
                            // completed and there's no `View` to update. Thus
                            // post a task to ensure an asynchronous context.
                            PostTask.postTask(
                                    TaskTraits.UI_DEFAULT,
                                    () -> {
                                        final ViewGroup v = (ViewGroup) getView();
                                        v.removeAllViews();
                                        v.addView(mErrorView);
                                    });
                            return;
                        }
                        showModule();
                    });

            top.addView(inflater.inflate(R.layout.cablev2_spinner, container, false));
            ((TextView) top.findViewById(R.id.status_text))
                    .setText(
                            getResources()
                                    .getString(R.string.cablev2_serverlink_status_dfm_install));

            final AnimatedVectorDrawableCompat anim =
                    AnimatedVectorDrawableCompat.create(
                            getContext(), R.drawable.circle_loader_animation);
            // There is no way to make an animation loop. Instead it must be
            // manually started each time it completes.
            anim.registerAnimationCallback(
                    new Animatable2Compat.AnimationCallback() {
                        @Override
                        public void onAnimationEnd(Drawable drawable) {
                            if (drawable != null) {
                                anim.start();
                            }
                        }
                    });
            ((ImageView) top.findViewById(R.id.spinner)).setImageDrawable(anim);
            anim.start();
        }

        return top;
    }

    @Override
    public void onClick(View v) {
        // The only button is the "Close" button on the error screen.
        getActivity().finish();
    }

    private void showModule() {
        FragmentTransaction transaction = getParentFragmentManager().beginTransaction();
        Fragment fragment = Cablev2AuthenticatorModule.getImpl().getFragment();
        Bundle arguments = getArguments();
        if (arguments == null) {
            arguments = new Bundle();
        }
        arguments.putLong(
                NETWORK_CONTEXT_KEY,
                CableAuthenticatorModuleProviderJni.get().getSystemNetworkContext());
        arguments.putLong(
                REGISTRATION_KEY, CableAuthenticatorModuleProviderJni.get().getRegistration());
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
    public static void onCloudMessage(byte[] serializedEvent, boolean isMakeCredential) {
        // Show a notification to the user. If tapped then an instance of this
        // class will be created in FCM mode.
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Intent intent;
        try {
            intent = new Intent(context, Class.forName(ACTIVITY_CLASS_NAME));
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Failed to find class " + ACTIVITY_CLASS_NAME);
            return;
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        Bundle bundle = new Bundle();
        bundle.putBoolean("org.chromium.chrome.modules.cablev2_authenticator.FCM", true);
        bundle.putByteArray(
                "org.chromium.chrome.modules.cablev2_authenticator.EVENT", serializedEvent);
        intent.putExtra("show_fragment_args", bundle);
        // Notifications must have a process-global ID. We never use this, but it prevents multiple
        // notifications from existing at once.
        final int id = 424386536;

        PendingIntent pendingIntent =
                PendingIntent.getActivity(
                        context,
                        id,
                        intent,
                        PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        String title = null;
        String body = null;
        if (isMakeCredential) {
            title = resources.getString(R.string.cablev2_make_credential_notification_title);
            body = resources.getString(R.string.cablev2_make_credential_notification_body);
        } else {
            title = resources.getString(R.string.cablev2_get_assertion_notification_title);
            body = resources.getString(R.string.cablev2_get_assertion_notification_body);
        }

        Notification notification =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.SECURITY_KEY)
                        .setAutoCancel(true)
                        .setCategory(Notification.CATEGORY_MESSAGE)
                        .setContentIntent(pendingIntent)
                        .setContentText(body)
                        .setContentTitle(title)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_MAX)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTimeoutAfter(NOTIFICATION_TIMEOUT_SECS * 1000)
                        .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                        .build();

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.notify(
                NotificationConstants.NOTIFICATION_ID_SECURITY_KEY, notification);
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
