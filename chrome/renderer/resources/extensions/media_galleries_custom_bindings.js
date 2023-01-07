// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Media Gallery API.

var blobNatives = requireNative('blob_natives');
var mediaGalleriesNatives = requireNative('mediaGalleries');

var blobsAwaitingMetadata = {};
var mediaGalleriesMetadata = {};

function createFileSystemObjectsAndUpdateMetadata(response) {
  var result = [];
  mediaGalleriesMetadata = {};  // Clear any previous metadata.
  if (response) {
    for (var i = 0; i < response.length; i++) {
      var filesystem = mediaGalleriesNatives.GetMediaFileSystemObject(
          response[i].fsid);
      $Array.push(result, filesystem);
      var metadata = response[i];
      delete metadata.fsid;
      mediaGalleriesMetadata[filesystem.name] = metadata;
    }
  }
  return result;
}

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems and addUserSelectedFolder use a custom callback so that
  // they can instantiate and return an array of file system objects.
  apiFunctions.setCustomCallback('getMediaFileSystems',
                                 function(callback, response) {
    var result = createFileSystemObjectsAndUpdateMetadata(response);
    if (callback)
      callback(result);
  });

  apiFunctions.setCustomCallback('addUserSelectedFolder',
      function(callback, response) {
    var fileSystems = [];
    var selectedFileSystemName = "";
    if (response && 'mediaFileSystems' in response &&
        'selectedFileSystemIndex' in response) {
      fileSystems = createFileSystemObjectsAndUpdateMetadata(
          response['mediaFileSystems']);
      var selectedFileSystemIndex = response['selectedFileSystemIndex'];
      if (selectedFileSystemIndex >= 0) {
        selectedFileSystemName = fileSystems[selectedFileSystemIndex].name;
      }
    }
    if (callback)
      callback(fileSystems, selectedFileSystemName);
  });

  apiFunctions.setHandleRequest('getMediaFileSystemMetadata',
                                function(filesystem) {
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

  function getMetadataCallback(uuid, callback, response, blobs) {
    if (response && blobs)
      response.metadata.attachedImages = blobs;

    if (callback)
      callback(response ? response.metadata : null);

    delete blobsAwaitingMetadata[uuid];
  }

  apiFunctions.setHandleRequest('getMetadata',
                                function(mediaFile, options, callback) {
    var blobUuid = blobNatives.GetBlobUuid(mediaFile)
    // Store the blob in a global object to keep its refcount nonzero -- this
    // prevents the object from being garbage collected before any metadata
    // parsing gets to occur (see crbug.com/415792).
    blobsAwaitingMetadata[blobUuid] = mediaFile;

    var optArgs = {
      __proto__: null,
      customCallback: $Function.bind(getMetadataCallback, null, blobUuid),
    };

    bindingUtil.sendRequest(
        'mediaGalleries.getMetadata', [blobUuid, options, callback],
        optArgs);
  });
});
