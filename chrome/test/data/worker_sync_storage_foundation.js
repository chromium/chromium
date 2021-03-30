// Copyright 2021 The Chromium Authors. All rights reserved.
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
