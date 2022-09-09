// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing erroneous_module3.js')

class TestOperation {
  run(data) {}
}

class TestURLSelectionOperation {
  run(urls, data) {}
}

register("test-operation", TestOperation);
register("test-url-selection-operation", TestURLSelectionOperation);

console.log('Finish executing erroneous_module3.js')
