// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing erroneous_module5.js')

class TestOperation {
  async run(data) {
    if (data.customField != 'customValue') {
       throw 'Unexpected value for customField field';
    }
  }
}

class TestURLSelectionOperation {
  async run(urls, data) {
    if (data.customField != 'customValue') {
       throw 'Unexpected value for customField field';
    }

    return 0;
  }
}

register("test-operation", TestOperation);
register("test-url-selection-operation", TestURLSelectionOperation);

console.log('Finish executing erroneous_module5.js')
