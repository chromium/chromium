// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(rdevlin.cronin): This file can be improved, and was copied from public
// docs. Clean both up.

// A generic onclick callback function.
function genericOnClick(info, tab) {
  console.log("item " + info.menuItemId + " was clicked");
  console.log("info: " + JSON.stringify(info));
  console.log("tab: " + JSON.stringify(tab));
}

// Create one test item for each context type.
var contexts = ["page","selection","link","editable","image","video",
                "audio"];
for (var i = 0; i < contexts.length; i++) {
  var context = contexts[i];
  var title = "Test '" + context + "' menu item";
  var id = chrome.contextMenus.create({"title": title, "contexts":[context],
                                       "onclick": genericOnClick});
  console.log("'" + context + "' item:" + id);
}


// Create a parent item and two children.
var parent = chrome.contextMenus.create({"title": "Test parent item"});
var child1 = chrome.contextMenus.create(
  {"title": "Child 1", "parentId": parent, "onclick": genericOnClick});
var child2 = chrome.contextMenus.create(
  {"title": "Child 2", "parentId": parent, "onclick": genericOnClick});
console.log("parent:" + parent + " child1:" + child1 + " child2:" + child2);


// Create some radio items.
function radioOnClick(info, tab) {
  console.log("radio item " + info.menuItemId +
              " was clicked (previous checked state was "  +
              info.wasChecked + ")");
}
var radio1 = chrome.contextMenus.create({"title": "Radio 1", "type": "radio",
                                         "onclick":radioOnClick});
var radio2 = chrome.contextMenus.create({"title": "Radio 2", "type": "radio",
                                         "onclick":radioOnClick});
console.log("radio1:" + radio1 + " radio2:" + radio2);


// Create some checkbox items.
function checkboxOnClick(info, tab) {
  console.log(JSON.stringify(info));
  console.log("checkbox item " + info.menuItemId +
              " was clicked, state is now: " + info.checked +
              "(previous state was " + info.wasChecked + ")");

}
var checkbox1 = chrome.contextMenus.create(
  {"title": "Checkbox1", "type": "checkbox", "onclick":checkboxOnClick});
var checkbox2 = chrome.contextMenus.create(
  {"title": "Checkbox2", "type": "checkbox", "onclick":checkboxOnClick});
console.log("checkbox1:" + checkbox1 + " checkbox2:" + checkbox2);


// Intentionally create an invalid item, to show off error checking in the
// create callback.
console.log("About to try creating an invalid item - an error about " +
            "item 999 should show up");
chrome.contextMenus.create({"title": "Oops", "parentId":999}, function() {
  if (chrome.extension.lastError) {
    console.log("Got expected error: " + chrome.extension.lastError.message);
  }
});
