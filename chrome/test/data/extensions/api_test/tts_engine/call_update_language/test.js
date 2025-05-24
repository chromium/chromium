// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testUpdateLanguageApi() {
  const status = {lang: 'fr', installStatus: 'installing'};
  chrome.ttsEngine.updateLanguage(status);
  chrome.test.succeed();
}]);
