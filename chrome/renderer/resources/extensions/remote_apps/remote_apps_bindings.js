// Copyright 2020 The Chromium Authors. All rights reserved.
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
    factory.create(
        this.remoteApps_.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Adds a folder to the launcher. Note that empty folders are not shown in
   * the launcher.
   * @param {string} name name of the added folder
   * @param {boolean} [add_to_front=false] true if the folder should be added
   *     to the front of the app list. Defaults to false.
   * @return {!Promise<!{folderId: string, error: string}>} ID for the added
   *     folder
   */
  addFolder(name, add_to_front = false) {
    return this.remoteApps_.addFolder(name, add_to_front);
  }

  /**
   * Adds an app to the launcher.
   * @param {string} name name of the added app
   * @param {string} folderId Id of the parent folder. An empty string
   *     indicates the app does not have a parent folder.
   * @param {string} iconUrl URL to an image representing the app's icon
   * @param {boolean} [add_to_front=false] true if the app should be added to
   *     the front of the app list. Defaults to false. Has no effect if the app
   *     has a parent folder.
   * @return {!Promise<!{appId: string, error: string}>} ID for the
   *     added app.
   */
  addApp(name, folderId, iconUrl, add_to_front = false) {
    return this.remoteApps_.addApp(
        name, folderId, {url: iconUrl}, add_to_front);
  }

  /**
   * Adds a callback for remote app launch events.
   * @param {function(string)} callback called when a remote app is launched
   *     with the app ID as argument.
   * @return {!Promise<void>}
   */
  addRemoteAppLaunchObserver(callback) {
    return this.callbackRouter_.onRemoteAppLaunched.addListener(callback);
  }
}

exports.$set('returnValue', new RemoteAppsAdapter());
