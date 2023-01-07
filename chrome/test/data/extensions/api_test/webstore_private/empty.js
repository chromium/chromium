// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var manifest = getManifest();
// Begin installing.
chrome.webstorePrivate.beginInstallWithManifest3(
    {'id': extensionId, 'manifest': manifest},
    function(result) {
      assertNoLastError();
      assertEq(result, "");

      // Now complete the installation.
      var expectedError = "Package is invalid: 'CRX_HEADER_INVALID'.";
      chrome.webstorePrivate.completeInstall(extensionId,
                                             callbackFail(expectedError));
});
