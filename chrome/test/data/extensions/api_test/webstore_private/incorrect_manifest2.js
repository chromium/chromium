// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parse the manifest, and delete the permissions key.
const manifestObj = JSON.parse(getManifest());
delete manifestObj['permissions'];

const manifest = JSON.stringify(manifestObj);

// Now do the install - we should get an install error because the actual crx
// had more permissions than the manifest we passed in to
// beginInstallWithManifest3.
chrome.webstorePrivate.beginInstallWithManifest3(
    {'id': extensionId, 'manifest': manifest}, function(result) {
      assertNoLastError();
      assertEq('', result);

      const expectedError = 'Manifest file is invalid';
      chrome.webstorePrivate.completeInstall(
          extensionId, callbackFail(expectedError));
    });
