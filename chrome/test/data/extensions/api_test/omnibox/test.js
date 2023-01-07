// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var incognito = chrome.extension.inIncognitoContext;
var incognitoSuffix = incognito ? " incognito" : "";

chrome.omnibox.onInputChanged.addListener(
  function(text, suggest) {
    chrome.test.log("onInputChanged: " + text);
    if (text == "suggestio") {
      // First test, complete "suggestio"
      var desc = 'Description with style: <match>&lt;match&gt;</match>, ' +
                 '<dim>[dim]</dim>, <url>(url till end)</url>';
      suggest([
        {content: text + "n1", description: desc},
        {content: text + "n2", description: "description2"},
        {content: text + "n3" + incognitoSuffix, description: "description3"},
      ]);
    } else if("d") {
      suggest([
        {content: "n1", description: "des1", deletable: true},
        {content: "n2", description: "des2", deletable: false},
      ]);
    } else {
      // Other tests, just provide a simple suggestion.
      suggest([{content: text + " 1", description: "description"}]);
    }
  });

chrome.omnibox.onInputEntered.addListener(
  function(text, disposition) {
    if (disposition == "newForegroundTab") {
      chrome.test.assertEq("newtab" + incognitoSuffix, text);
      chrome.test.notifyPass();
    } else {
      chrome.test.assertEq("command" + incognitoSuffix, text);
      chrome.test.notifyPass();
    }
  });

chrome.omnibox.onDeleteSuggestion.addListener(
  function(text) {
    chrome.test.sendMessage("onDeleteSuggestion: " + text);
  });

// Now we wait for the input events to fire.
chrome.test.notifyPass();
