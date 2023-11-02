// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ghazale): There's no good way of checking that the import
// succeeds, so in the failure case the test just times out, rather than
// giving a useful error.
import {pass as jsPass} from '/module.js';

chrome.test.runTests([
  function jsModule() {
    jsPass();
  },
]);
