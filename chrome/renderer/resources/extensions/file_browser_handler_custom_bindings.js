// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileBrowserHandler API.

var fileBrowserNatives = requireNative('file_browser_handler');
var GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;
var fileBrowserHandlerInternal = getInternalApi('fileBrowserHandlerInternal');

bindingUtil.registerEventArgumentMassager('fileBrowserHandler.onExecute',
                                          function(args, dispatch) {
  if (args.length < 2) {
    dispatch(args);
    return;
  }
  var fileList = args[1].entries;
  if (!fileList) {
    dispatch(args);
    return;
  }
  // The second parameter for this event's payload is file definition
  // dictionary that we used to reconstruct File API's Entry instance
  // here.
  for (var i = 0; i < fileList.length; i++)
    fileList[i] = GetExternalFileEntry(fileList[i]);
  dispatch(args);
});

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('selectFile',
                                function(selectionParams, callback) {
    function internalCallback(externalCallback, internalResult) {
      if (!externalCallback)
        return;
      var result = undefined;
      if (internalResult) {
        result = { success: internalResult.success, entry: null };
        if (internalResult.success)
          result.entry = GetExternalFileEntry(internalResult.entry);
      }

      externalCallback(result);
    }

    return fileBrowserHandlerInternal.selectFile(
        selectionParams, $Function.bind(internalCallback, null, callback));
  });
});
