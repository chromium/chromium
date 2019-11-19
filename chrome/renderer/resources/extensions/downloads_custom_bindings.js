// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the downloads API.

var downloadsInternal = getInternalApi('downloadsInternal');

bindingUtil.registerEventArgumentMassager('downloads.onDeterminingFilename',
                                          function(args, dispatch) {
  var downloadItem = args[0];
  // Copy the id so that extensions can't change it.
  var downloadId = downloadItem.id;
  var suggestable = true;
  function isValidResult(result) {
    if (result === undefined)
      return false;
    if (typeof(result) != 'object') {
      console.error('Error: Invocation of form suggest(' + typeof(result) +
                    ') doesn\'t match definition suggest({filename: string, ' +
                    'conflictAction: string})');
      return false;
    } else if ((typeof(result.filename) != 'string') ||
               (result.filename.length == 0)) {
      console.error('Error: "filename" parameter to suggest() must be a ' +
                    'non-empty string');
      return false;
    } else if ([undefined, 'uniquify', 'overwrite', 'prompt'].indexOf(
                 result.conflictAction) < 0) {
      console.error('Error: "conflictAction" parameter to suggest() must be ' +
                    'one of undefined, "uniquify", "overwrite", "prompt"');
      return false;
    }
    return true;
  }
  function suggestCallback(result) {
    if (!suggestable) {
      console.error('suggestCallback may not be called more than once.');
      return;
    }
    suggestable = false;
    if (isValidResult(result)) {
      downloadsInternal.determineFilename(
          downloadId, result.filename, result.conflictAction || "");
    } else {
      downloadsInternal.determineFilename(downloadId, "", "");
    }
  }
  try {
    var results = dispatch([downloadItem, suggestCallback]);
    var async = (results &&
                 results.results &&
                 (results.results.length != 0) &&
                 (results.results[0] === true));
    if (suggestable && !async)
      suggestCallback();
  } catch (e) {
    suggestCallback();
    throw e;
  }
});
