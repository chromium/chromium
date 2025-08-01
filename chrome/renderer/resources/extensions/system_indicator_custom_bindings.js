// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the systemIndicator API.
// TODO(dewittj) Refactor custom binding to reduce redundancy between the
// extension action APIs.

const getSetIconHandler = require('setIcon').getSetIconHandler;

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest(
      'setIcon', getSetIconHandler('systemIndicator.setIcon'));
});
