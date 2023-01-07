// Copyright 2021 The Chromium Authors
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
}
