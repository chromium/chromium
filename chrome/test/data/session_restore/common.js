// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTest() {
  var old_value = readData();
  if (!writeData())
    return 'ERROR_WRITE_FAILED';
  var new_value = readData();
  if (new_value == '')
    return 'ERROR_EMPTY';
  if (old_value === new_value)
    return 'PASS';
  return 'STORING';
}

function onLoad() {
  document.title = runTest();
}
