// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// store some stuff in local storage
localStorage.foo = "bar";

console.log("Opening database...");
// store some stuff in a database
var db = window.openDatabase("mydb2", "1.0", "database test", 2048);
if (!db)
  chrome.test.notifyFail("failed to open database");

console.log("Performing transaction...");
db.transaction(function(tx) {
  tx.executeSql("drop table if exists note", [], onDropSuccess, onSqlError);
  tx.executeSql("create table note (body text)", [], onCreateSuccess, onSqlError);
  tx.executeSql("insert into note values ('hotdog')", [], onSqlExecuted,
                onSqlError);
}, function(error) {
  chrome.test.notifyFail(error.message);
});

function onDropSuccess(tx, res) {
  console.log("note table dropped");
}
function onCreateSuccess(tx, res) {
  console.log("note table created");
}
function onSqlError(tx, error) {
  chrome.test.notifyFail(error.message);
}
function onSqlExecuted() {
  console.log("Opening tab...");
  // Open a tab. This doesn't really prove we're writing to disk, but it is
  // difficult to prove that without shutting down the process. We'll just
  // trust that if this next trick works, that the real testing for local
  // storage is good enough to catch more subtle errors.
  chrome.tabs.create({
    url: "tab.html"
  });
}
