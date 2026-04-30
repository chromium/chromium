// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileSystem = null;

console.info('Requesting a filesystem...');
webkitRequestFileSystem(window.TEMPORARY, 100, getFileSystem, errorCallback);

function getFileSystem(fs) {
  fileSystem = fs;
  console.info(`DONE requesting filesystem: ${fileSystem.name}`);
  fileSystem.root.getDirectory(
      'dir', {create: true}, directoryCallback, errorCallback);
}

function directoryCallback(directory) {
  console.info(`DONE creating directory: ${directory.path}`);
  directory.getFile('file', {create: true}, fileCallback, errorCallback);
}

function fileCallback(file) {
  console.info(`DONE creating file: ${file.path}`);

  // See if we get the same filesystem space in the tab.
  console.info('Opening tab...');
  chrome.tabs.create({
    url: 'tab.html',
  });
}

function errorCallback(error) {
  chrome.test.fail(`Got unexpected error: ${error.code}`);
}
