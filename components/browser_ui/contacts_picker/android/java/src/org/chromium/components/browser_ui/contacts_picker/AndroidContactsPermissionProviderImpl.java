// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ContactsFetcher;
import org.chromium.content_public.browser.ContactsPermissionProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of {@link ContactsPermissionProvider} that asks permission to Android Framework.
 */
@NullMarked
public class AndroidContactsPermissionProviderImpl implements ContactsPermissionProvider {
    @Override
    public void run(WebContents webContents, Callback callback) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        assert windowAndroid != null;
        Context context = windowAndroid.getContext().get();
        assumeNonNull(context);
        ContentResolver contentResolver = context.getContentResolver();
        ContactsFetcher contactsFetcher = new AndroidContactsFetcherImpl(contentResolver);

        if (windowAndroid.getActivity().get() == null) {
            callback.onDenied();
            return;
        }

        if (windowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)) {
            callback.onAllowed(contactsFetcher);
            return;
        }

        if (!windowAndroid.canRequestPermission(Manifest.permission.READ_CONTACTS)) {
            callback.onDenied();
            return;
        }

        windowAndroid.requestPermissions(
                new String[] {Manifest.permission.READ_CONTACTS},
                (permissions, grantResults) -> {
                    if (permissions.length == 1
                            && grantResults.length == 1
                            && TextUtils.equals(permissions[0], Manifest.permission.READ_CONTACTS)
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        callback.onAllowed(contactsFetcher);
                    } else {
                        callback.onDenied();
                    }
                });
    }
}
