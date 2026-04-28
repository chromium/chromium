// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The id of the extension in localized_extension.crx.
const localizedId = 'oolblhbomdbcpmafphaodhjfcgbihcdg';

const tests = [

  // This tests an install passing a localized name.
  function localizeName() {
    // See things through all the way to a successful install.
    listenOnce(chrome.management.onInstalled, function(info) {
      assertEq(info.id, localizedId);
    });

    const manifest = getManifest('localized_extension/manifest.json');
    const messages =
        getManifest('localized_extension/_locales/fr/messages.json');
    // Begin installing.
    chrome.webstorePrivate.beginInstallWithManifest3(
        {
          'id': localizedId,
          'manifest': manifest,
          'localizedName': 'Le Title',
          'locale': 'fr',
        },
        callbackPass(function(result) {
          assertEq(result, '');
          chrome.webstorePrivate.completeInstall(localizedId, callbackPass());
        }));
  },

];

runTests(tests);
