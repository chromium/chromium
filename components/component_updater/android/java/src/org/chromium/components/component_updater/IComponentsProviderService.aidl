// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.component_updater;

import android.os.ResultReceiver;

interface IComponentsProviderService {
    /**
     * Request files for a component. The caller should expect that files may be missing and should
     * handle that.
     *
     * This is an asynchronous call and there is no guarantee on the order of execution of multiple
     * calls. The caller should not unbind the service until all ResultReceivers are called.
     *
     * The componentId is the unique name of the component as returned by
     * ComponentInstallerPolicy#getHash.
     *
     * On success, the resultReceiver will be called with a return code of zero and the resultData
     * Bundle will contain a HashMap that maps file paths (relative to their component directory) to
     * an open ParcelFileDescriptor. The caller is responsible for closing these file descriptors.
     * The result map can be retrieved by calling
     * resultData.getSerializable(ComponentsProviderService.KEY_RESULT) and casting the result to a
     * (HashMap<String, ParcelFileDescriptor>).
     * On failure, a non-zero result code is sent with a null Bundle.
     *
     * @param componentId the component for which to retrieve files
     * @param resultReceiver a callback to receive the result
     */
    oneway void getFilesForComponent(String componentId, in ResultReceiver resultReceiver);

    // TODO(crbug.com/1178769): this is specific to WebView's implementation and should be moved
    // into android_webview.
    /**
     * Called by the component installer to notify the service that a new version is installed. This
     * method may only be called from WebView UID, otherwise a SecurityException will be thrown.
     *
     * This is a synchronous call. The service will move component files from the installPath into
     * its own directory and subsequent file requests for this component will serve this version. If
     * another version was installed, it will be deleted after the new version is moved into place.
     * If version is the same as the currently installed version, this is a noop.
     *
     * The componentId is the unique name of the component as returned by
     * ComponentInstallerPolicy#getHash. The installPath is the absolute path of a writable
     * directory containing the component files.
     *
     * @param componentId the component to be updated or installed
     * @param installPath the absolute path to this version's component files
     * @param version to be installed
     *
     * @return true on success, false otherwise (for example if the service failed to move files)
     */
    boolean onNewVersion(String componentId, String installPath, String version);
}
