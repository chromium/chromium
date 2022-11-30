// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createFile() {
  webkitRequestFileSystem(window.PERSISTENT, 1024, gotFS, fail);
};

function gotFS(fs) {
  fs.root.getFile("hoge", {create: true, exclusive: false}, gotFileEntry, fail);
}

function gotFileEntry(entry) {
  entry.createWriter(gotWriter.bind(null, entry), fail);
}

function gotWriter(entry, writer) {
  writer.write(new Blob(["fuga"]));
  writer.onwrite = didWrite.bind(null, entry);
  writer.onerror = fail;
}

function didWrite(entry) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", entry.toURL());
  xhr.send();
  xhr.onload = pass;
  xhr.onerror = fail;
}

function pass() {
  if (window.chrome && chrome.test && chrome.test.succeed)
    chrome.test.succeed();
  document.body.innerText = "PASS";
}

function fail() {
  if (window.chrome && chrome.test && chrome.test.fail)
    chrome.test.fail();
  document.body.innerText = "FAIL";
}

createFile();
