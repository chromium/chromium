// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var manifestObj = JSON.parse(getManifest());

// Change the version to be N+1.
manifestObj["version"] =
    (parseFloat(manifestObj["version"]) + 1).toString();
var manifest = JSON.stringify(manifestObj);

// Now cause the install to happen - it should succeed even though the version
// we passed in is different from what is in the .crx file, because the
// effective permissions have not changed.
chrome.webstorePrivate.beginInstallWithManifest3(
    { 'id': extensionId, 'manifest': manifest },
    function(result) {
  assertNoLastError();
  assertEq("", result);

  chrome.webstorePrivate.completeInstall(extensionId,
                                         callbackPass());
});
