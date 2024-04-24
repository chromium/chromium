// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing module_with_custom_header.js')

class TestOperation {
  async run(data) {
    console.log('Start executing \'test-operation\'');
    console.log(JSON.stringify(data, Object.keys(data).sort()));
    console.log('Finish executing \'test-operation\'');
  }
}

class TestURLSelectionOperation {
  async run(urls, data) {
    console.log('Start executing \'test-url-selection-operation\'');
    console.log(JSON.stringify(urls));
    console.log(JSON.stringify(data, Object.keys(data).sort()));
    console.log('Finish executing \'test-url-selection-operation\'');

    if (data && data.hasOwnProperty('mockResult')) {
      return data['mockResult'];
    }

    return -1;
  }
}

register('test-operation', TestOperation);
register('test-url-selection-operation', TestURLSelectionOperation);

console.log('Finish executing module_with_custom_header.js')
