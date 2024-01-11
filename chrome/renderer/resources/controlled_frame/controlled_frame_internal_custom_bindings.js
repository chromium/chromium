// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var contextMenusHandlers = require('contextMenusHandlers');

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var handlers = contextMenusHandlers.create(
      /*webViewNamespace=*/'controlledFrameInternal');

  apiFunctions.setHandleRequest(
      'contextMenusCreate', handlers.requestHandlers.create);
});
