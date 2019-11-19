// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addLog(logText) {
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  logDiv.appendChild(document.createElement('hr'));
  const textDiv = document.createElement('div');
  textDiv.innerText = logText;
  logDiv.appendChild(textDiv);
}

// Converts an internal representation of nodes to actual DOM nodes that can
// be attached to the DOM. The internal representation has the following
// properties for each node:
// - type: 'element' | 'text'
// - value: name of tag | text content
// - children (opt): list of child nodes
// - attributes (opt): dictionary of name/value pairs
function nodeToDomNode(node) {
  if (node.type === 'text') {
    return document.createTextNode(node.value);
  }
  // Else the node is of type 'element'.
  const domNode = document.createElement(node.value);
  if ('children' in node) {
    node.children.forEach((child) => {
      domNode.appendChild(nodeToDomNode(child));
    });
  }
  if ('attributes' in node) {
    for (const attribute in node.attributes) {
      domNode.setAttribute(attribute, node.attributes[attribute]);
    }
  }
  return domNode;
}

// TODO(crbug.com/928595) Rename this to "addStructuredLog". Punting on this
// to simplify an existing CL that shall be merged.
function addRawLog(node) {
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  logDiv.appendChild(document.createElement('hr'));
  if (node.type === 'fragment') {
    if ('children' in node) {
      node.children.forEach((child) => {
        logDiv.appendChild(nodeToDomNode(child));
      });
    }
  } else {
    logDiv.appendChild(nodeToDomNode(node));
  }
}

function setUpAutofillInternals() {
    document.title = "Autofill Internals";
    document.getElementById("h1-title").innerHTML = "Autofill Internals";
    document.getElementById("logging-note").innerText =
      "Captured autofill logs are listed below. Logs are cleared and no longer \
      captured when all autofill-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured autofill logs are not available in Incognito.";
}

function setUpPasswordManagerInternals() {
    document.title = "Password Manager Internals";
    document.getElementById("h1-title").innerHTML =
      "Password Manager Internals";
    document.getElementById("logging-note").innerText =
      "Captured password manager logs are listed below. Logs are cleared and \
      no longer captured when all password-manager-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured password manager logs are not available in Incognito.";
}

function notifyAboutIncognito(isIncognito) {
  document.body.dataset.incognito = isIncognito;
}

function notifyAboutVariations(variations) {
  const list = document.createElement("div");
  for (const item of variations) {
    list.appendChild(document.createTextNode(item));
    list.appendChild(document.createElement("br"));
  }
  const variationsList = document.getElementById("variations-list");
  variationsList.appendChild(list);
}

document.addEventListener("DOMContentLoaded", function(event) {
  chrome.send('loaded');
});
