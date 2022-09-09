// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing erroneous_module6.js')

class TestURLSelectionOperation1 {
  async run(urls, data) {
    return 1;
  }
}

class TestURLSelectionOperation2 {
  async run(urls, data) {
    class CustomClass {
      toString() { throw Error('error 123'); }
    }

    return new CustomClass();
  }
}

register("test-url-selection-operation-1", TestURLSelectionOperation1);
register("test-url-selection-operation-2", TestURLSelectionOperation2);

console.log('Finish executing erroneous_module6.js')
