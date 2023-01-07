// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = function(e) {
  var result = 'Success'

  try {
    switch(e.data) {
      case 'openSync':
          storageFoundation.openSync('foo');
        break;
      case 'deleteSync':
          storageFoundation.deleteSync('foo');
        break;
      case 'renameSync':
          storageFoundation.renameSync('foo', 'bar');
        break;
      case 'getAllSync':
          storageFoundation.getAllSync();
        break;
      case 'requestCapacitySync':
          storageFoundation.requestCapacitySync(10);
        break;
      case 'releaseCapacitySync':
          storageFoundation.releaseCapacitySync(10);
        break;
      case 'getRemainingCapacitySync':
          storageFoundation.getRemainingCapacitySync();
        break;
      default:
        result = 'unknown message received'
    }
  } catch (e) {
      result = getErrorMessage(e);
  }

  postMessage(e.data + ' - ' + result);
}

function getErrorMessage(e) {
  const error = e.toString();
  const n = error.lastIndexOf(`: `);
  return error.substring(n + 2);
}
