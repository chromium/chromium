// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings_lite');
}

loadScript('url/mojom/url.mojom-lite');
loadScript('chromeos.remote_apps.mojom-lite');

class RemoteAppsAdapter {
  constructor() {
    const factory = chromeos.remoteApps.mojom.RemoteAppsFactory.getRemote();

    this.remoteApps_ = new chromeos.remoteApps.mojom.RemoteAppsRemote();
    this.callbackRouter_ =
        new chromeos.remoteApps.mojom.RemoteAppLaunchObserverCallbackRouter();
    factory.bindRemoteAppsAndAppLaunchObserver(
        chrome.runtime.id, this.remoteApps_.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Adds a folder to the launcher. Note that empty folders are not shown in
   * the launcher.
   * @param {string} name name of the added folder
   * @param {boolean} [add_to_front=false] true if the folder should be added
   *     to the front of the app list. Defaults to false.
   * @return {!Promise<!{folderId?: string, error?: string}>} ID for the added
   *     folder
   */
  async addFolder(name, add_to_front = false) {
    const addFolderResult =
        await this.remoteApps_.addFolder(name, add_to_front);
    return addFolderResult.result;
  }

  /**
   * Adds an app to the launcher.
   * @param {string} name name of the added app
   * @param {string} folderId ID of the parent folder. An empty string
   *     indicates the app does not have a parent folder.
   * @param {string} iconUrl URL to an image representing the app's icon
   * @param {boolean} [add_to_front=false] true if the app should be added to
   *     the front of the app list. Defaults to false. Has no effect if the app
   *     has a parent folder.
   * @return {!Promise<!{appId?: string, error?: string}>} ID for the
   *     added app.
   */
  async addApp(name, folderId, iconUrl, add_to_front = false) {
    const addAppResult = await this.remoteApps_.addApp(
        chrome.runtime.id, name, folderId, {url: iconUrl}, add_to_front);
    return addAppResult.result;
  }

  /**
   * Deletes an app that was previously added by |addApp()|. If the app was in
   * a folder and the folder would become empty, the folder is hidden.
   * @param {string} appId ID of the app to delete.
   * @return {!Promise<!{error: string}>} error if any.
   */
  deleteApp(appId) {
    return this.remoteApps_.deleteApp(appId);
  }

  /**
   * Adds a callback for remote app launch events.
   * @param {function(string)} callback called when a remote app is launched
   *     with the app ID as argument.
   * @return {!Promise<void>}
   */
  addRemoteAppLaunchObserver(callback) {
    // The second parameter from the |OnRemoteAppLaunched| Mojo method,
    // |source_id|, is dropped.
    return this.callbackRouter_.onRemoteAppLaunched.addListener(
        (app_id) => callback(app_id));
  }
}

exports.$set('returnValue', new RemoteAppsAdapter());
