// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Media Gallery API.

const mediaGalleriesNatives = requireNative('mediaGalleries');

let mediaGalleriesMetadata = {};

function createFileSystemObjectsAndUpdateMetadata(response) {
  const result = [];
  mediaGalleriesMetadata = {};  // Clear any previous metadata.
  if (response) {
    for (let i = 0; i < response.length; i++) {
      const filesystem =
          mediaGalleriesNatives.GetMediaFileSystemObject(response[i].fsid);
      $Array.push(result, filesystem);
      const metadata = response[i];
      delete metadata.fsid;
      mediaGalleriesMetadata[filesystem.name] = metadata;
    }
  }
  return result;
}

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  const apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems and addUserSelectedFolder use a custom callback so that
  // they can instantiate and return an array of file system objects.
  apiFunctions.setCustomCallback(
      'getMediaFileSystems', function(callback, response) {
        const result = createFileSystemObjectsAndUpdateMetadata(response);
        if (callback) {
          callback(result);
        }
      });

  apiFunctions.setCustomCallback(
      'addUserSelectedFolder', function(callback, response) {
        let fileSystems = [];
        let selectedFileSystemName = '';
        if (response && 'mediaFileSystems' in response &&
            'selectedFileSystemIndex' in response) {
          fileSystems = createFileSystemObjectsAndUpdateMetadata(
              response['mediaFileSystems']);
          const selectedFileSystemIndex = response['selectedFileSystemIndex'];
          if (selectedFileSystemIndex >= 0) {
            selectedFileSystemName = fileSystems[selectedFileSystemIndex].name;
          }
        }
        if (callback) {
          callback(fileSystems, selectedFileSystemName);
        }
      });

  apiFunctions.setHandleRequest(
      'getMediaFileSystemMetadata', function(filesystem) {
        if (filesystem && filesystem.name &&
            filesystem.name in mediaGalleriesMetadata) {
          return mediaGalleriesMetadata[filesystem.name];
        }
        return {
          'name': '',
          'galleryId': '',
          'isRemovable': false,
          'isMediaDevice': false,
          'isAvailable': false,
        };
      });
});
