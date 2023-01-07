// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function makeAbsoluteUrl(path) {
  var parts = location.href.split("/");
  parts.pop();
  parts.push(path);
  return parts.join("/");
}

// The |absoluteIconUrl| parameter controls whether to use a relative or
// absolute url for the test.
function runSuccessTest(absoluteIconUrl, manifest) {
  var iconPath = "extension/icon.png";
  var iconUrl = absoluteIconUrl ? makeAbsoluteUrl(iconPath) : iconPath;
  installAndCleanUp(
      {'id': extensionId,'iconUrl': iconUrl, 'manifest': manifest },
      function() {});
}

var tests = [
  function IconUrlFailure() {
    var manifest = getManifest();
    var loadFailureUrl = makeAbsoluteUrl("does_not_exist.png");
    chrome.webstorePrivate.beginInstallWithManifest3(
        {'id': extensionId,'iconUrl': loadFailureUrl, 'manifest': manifest },
        callbackFail("Image decode failed", function(result) {
      assertEq("icon_error", result);
    }));
  },

  function IconUrlSuccess() {
    var manifest = getManifest();
    runSuccessTest(false, manifest);
  },

  function IconUrlSuccessAbsoluteUrl() {
    var manifest = getManifest();
    runSuccessTest(true, manifest);
  }
];

runTests(tests);
