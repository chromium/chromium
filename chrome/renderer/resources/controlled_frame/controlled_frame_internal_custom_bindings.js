// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const contextMenusHandlers = require('contextMenusHandlers');

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  const handlers = contextMenusHandlers.create(
      /*webViewNamespace=*/ 'controlledFrameInternal');

  apiFunctions.setHandleRequest(
      'contextMenusCreate', handlers.requestHandlers.create);

  apiFunctions.setHandleRequest(
      'contextMenusUpdate', handlers.requestHandlers.update);
});
