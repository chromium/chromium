// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.url.Origin;

/**
 * Initiates showing modal dialog asking user whether they want to share their identity with
 * website.
 */
public class DigitalIdentitySafetyInterstitialBridge {
    @VisibleForTesting public static final String DIGITAL_IDENTITY_DIALOG_PARAM = "dialog";

    @VisibleForTesting
    public static final String DIGITAL_IDENTITY_LOW_RISK_DIALOG_PARAM_VALUE = "low_risk";

    @VisibleForTesting
    public static final String DIGITAL_IDENTITY_HIGH_RISK_DIALOG_PARAM_VALUE = "high_risk";

    private long mNativeDigitalIdentitySafetyInterstitialBridgeAndroid;

    private DigitalIdentitySafetyInterstitialBridge(
            long digitalIdentitySafetyInterstitialBridgeAndroid) {
        mNativeDigitalIdentitySafetyInterstitialBridgeAndroid =
                digitalIdentitySafetyInterstitialBridgeAndroid;
    }

    @CalledByNative
    public static DigitalIdentitySafetyInterstitialBridge create(
            long digitalIdentitySafetyInterstitialBridgeAndroid) {
        return new DigitalIdentitySafetyInterstitialBridge(
                digitalIdentitySafetyInterstitialBridgeAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeDigitalIdentitySafetyInterstitialBridgeAndroid = 0;
    }

    @CalledByNative
    public void showInterstitialIfNeeded(
            WindowAndroid window, Origin origin, boolean isOnlyRequestingAge) {
        if (window == null) {
            onDone(DigitalIdentityRequestStatusForMetrics.ERROR_OTHER);
            return;
        }

        Activity activity = window.getActivity().get();
        ModalDialogManager modalDialogManager =
                (activity instanceof ModalDialogManagerHolder)
                        ? ((ModalDialogManagerHolder) activity).getModalDialogManager()
                        : null;

        if (modalDialogManager == null) {
            onDone(DigitalIdentityRequestStatusForMetrics.ERROR_OTHER);
            return;
        }

        boolean showLowRiskDialog = false;
        boolean showHighRiskDialog = !isOnlyRequestingAge;
        if (isOnlyRequestingAge) {
            String dialogParamValue =
                    ContentFeatureMap.getInstance()
                            .getFieldTrialParamByFeature(
                                    ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS,
                                    DIGITAL_IDENTITY_DIALOG_PARAM);
            showLowRiskDialog =
                    dialogParamValue.equals(DIGITAL_IDENTITY_LOW_RISK_DIALOG_PARAM_VALUE);
            showHighRiskDialog =
                    dialogParamValue.equals(DIGITAL_IDENTITY_HIGH_RISK_DIALOG_PARAM_VALUE);
        }

        if (!showLowRiskDialog && !showHighRiskDialog) {
            onDone(DigitalIdentityRequestStatusForMetrics.SUCCESS);
            return;
        }

        DigitalIdentitySafetyInterstitialController.show(
                modalDialogManager,
                /* isHighRisk= */ showHighRiskDialog,
                origin,
                (/*DialogDismissalCause*/ Integer dismissalCause) -> {
                    onDone(
                            dismissalCause.intValue()
                                            == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    ? DigitalIdentityRequestStatusForMetrics.SUCCESS
                                    : DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED);
                });
    }

    public void onDone(@DigitalIdentityRequestStatusForMetrics int statusForMetrics) {
        if (mNativeDigitalIdentitySafetyInterstitialBridgeAndroid != 0) {
            DigitalIdentitySafetyInterstitialBridgeJni.get()
                    .onInterstitialDone(
                            mNativeDigitalIdentitySafetyInterstitialBridgeAndroid,
                            statusForMetrics);
        }
    }

    @NativeMethods
    interface Natives {
        void onInterstitialDone(
                long nativeDigitalIdentitySafetyInterstitialBridgeAndroid,
                @DigitalIdentityRequestStatusForMetrics int statusForMetrics);
    }
}
