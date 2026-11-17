// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.consent.DriveConsentDialog;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Bridges the WebUI New Tab Page to the Android Drive file picker. */
@NullMarked
final class DrivePickerBridge {
    private final WindowAndroid mWindowAndroid;
    private final Profile mProfile;
    private @Nullable JniOnceCallback<List<DriveAttachmentMetadata>> mCallback;
    private @Nullable DriveConsentDialog mConsentDialog;

    private DrivePickerBridge(
            WindowAndroid windowAndroid,
            Profile profile,
            JniOnceCallback<List<DriveAttachmentMetadata>> callback) {
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mCallback = callback;
    }

    /** Checks whether Drive file picking is available on this device and profile. */
    @CalledByNative
    static boolean isAvailable(@JniType("Profile*") @Nullable Profile profile) {
        return DriveFilePickerClient.getInstance()
                .isAvailable(ContextUtils.getApplicationContext(), profile);
    }

    /**
     * Starts the Drive picker flow, presenting consent first if needed.
     *
     * @return The bridge instance, which C++ must destroy if it stops waiting for the result (e.g.
     *     the tab closes), or null if the flow already finished and the result was sent.
     */
    @CalledByNative
    static @Nullable DrivePickerBridge start(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("Profile*") Profile profile,
            boolean needsConsent,
            JniOnceCallback<List<DriveAttachmentMetadata>> callback) {
        DrivePickerBridge bridge = new DrivePickerBridge(windowAndroid, profile, callback);
        if (needsConsent) {
            bridge.mConsentDialog =
                    DriveConsentDialog.show(windowAndroid, profile, bridge::onConsentResult);
        } else {
            bridge.launchPicker();
        }
        return bridge.mCallback != null ? bridge : null;
    }

    /**
     * Closes any active consent dialog and destroys the pending callback without running it. The
     * Drive picker is a separate Activity and can't be closed; its result is dropped.
     */
    @CalledByNative
    void destroy() {
        JniOnceCallback<List<DriveAttachmentMetadata>> callback = mCallback;
        DriveConsentDialog dialog = mConsentDialog;
        mCallback = null;
        mConsentDialog = null;
        if (dialog != null) {
            dialog.destroy();
        }
        if (callback != null) {
            callback.destroy();
        }
    }

    /** Runs when the user accepts, declines, or dismisses the {@link DriveConsentDialog}. */
    private void onConsentResult(boolean granted) {
        mConsentDialog = null;
        // If `destroy()` was already called because the tab closed, ignore the result.
        if (mCallback == null) return;
        if (granted) {
            launchPicker();
        } else {
            sendResult(List.of());
        }
    }

    /** Opens the Android Google Drive app's file picker Activity and waits for the result. */
    private void launchPicker() {
        DriveFilePickerClient.getInstance()
                .launchPicker(
                        mWindowAndroid,
                        mProfile,
                        /* mimeTypes= */ null,
                        /* allowMultiSelect= */ true)
                .then(this::sendResult, unusedError -> sendResult(List.of()));
    }

    /**
     * Sends the picked files (or an empty list on cancel/error) back to C++ exactly once, then
     * clears {@code mCallback} so duplicate or late callbacks can never fire twice.
     */
    private void sendResult(List<DriveAttachmentMetadata> files) {
        JniOnceCallback<List<DriveAttachmentMetadata>> callback = mCallback;
        mCallback = null;
        mConsentDialog = null;
        if (callback != null) {
            callback.onResult(files);
        }
    }
}
