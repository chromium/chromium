// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the omnibox API. Only injected into the v8 contexts
// for extensions which have permission for the omnibox API.

// Remove invalid characters from |text| so that it is suitable to use
// for |AutocompleteMatch::contents|.
function sanitizeString(text, shouldTrim) {
  // NOTE: This logic mirrors |AutocompleteMatch::SanitizeString()|.
  // 0x2028 = line separator; 0x2029 = paragraph separator.
  var kRemoveChars = /(\r|\n|\t|\u2028|\u2029)/gm;
  if (shouldTrim)
    text = text.trimLeft();
  return text.replace(kRemoveChars, '');
}

// Parses the xml syntax supported by omnibox suggestion results. Returns an
// object with two properties: 'description', which is just the text content,
// and 'descriptionStyles', which is an array of style objects in a format
// understood by the C++ backend.
function parseOmniboxDescription(input) {
  var domParser = new DOMParser();

  // The XML parser requires a single top-level element, but we want to
  // support things like 'hello, <match>world</match>!'. So we wrap the
  // provided text in generated root level element.
  var root = domParser.parseFromString(
      '<fragment>' + input + '</fragment>', 'text/xml');

  // DOMParser has a terrible error reporting facility. Errors come out nested
  // inside the returned document.
  var error = root.querySelector('parsererror div');
  if (error) {
    throw new Error(error.textContent);
  }

  // Otherwise, it's valid, so build up the result.
  var result = {
    description: '',
    descriptionStyles: []
  };

  // Recursively walk the tree.
  function walk(node) {
    for (var i = 0, child; child = node.childNodes[i]; i++) {
      // Append text nodes to our description.
      if (child.nodeType == Node.TEXT_NODE) {
        var shouldTrim = result.description.length == 0;
        result.description += sanitizeString(child.nodeValue, shouldTrim);
        continue;
      }

      // Process and descend into a subset of recognized tags.
      if (child.nodeType == Node.ELEMENT_NODE &&
          (child.nodeName == 'dim' || child.nodeName == 'match' ||
           child.nodeName == 'url')) {
        var style = {
          'type': child.nodeName,
          'offset': result.description.length
        };
        $Array.push(result.descriptionStyles, style);
        walk(child);
        style.length = result.description.length - style.offset;
        continue;
      }

      // Descend into all other nodes, even if they are unrecognized, for
      // forward compat.
      walk(child);
    }
  };
  walk(root);

  return result;
}

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setUpdateArgumentsPreValidate('setDefaultSuggestion',
                                             function(suggestResult) {
    if (suggestResult.content != undefined) {  // null, etc.
      throw new Error(
          'setDefaultSuggestion cannot contain the "content" field');
    }
    return [suggestResult];
  });

  apiFunctions.setHandleRequest('setDefaultSuggestion', function(details) {
    var parseResult = parseOmniboxDescription(details.description);
    bindingUtil.sendRequest('omnibox.setDefaultSuggestion', [parseResult],
                            undefined);
  });

  apiFunctions.setUpdateArgumentsPostValidate(
      'sendSuggestions', function(requestId, userSuggestions) {
    var suggestions = [];
    for (var i = 0; i < userSuggestions.length; i++) {
      var parseResult = parseOmniboxDescription(
          userSuggestions[i].description);
      parseResult.content = userSuggestions[i].content;
      parseResult.deletable = userSuggestions[i].deletable;
      $Array.push(suggestions, parseResult);
    }
    return [requestId, suggestions];
  });
});

bindingUtil.registerEventArgumentMassager('omnibox.onInputChanged',
                                          function(args, dispatch) {
  var text = args[0];
  var requestId = args[1];
  var suggestCallback = function(suggestions) {
    chrome.omnibox.sendSuggestions(requestId, suggestions);
  };
  dispatch([text, suggestCallback]);
});
