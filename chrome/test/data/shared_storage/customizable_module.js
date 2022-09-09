// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing customizable_module.js')

class TestOperation {
  async run(data) {
    {{RUN_FUNCTION_BODY}}
  }
}

register("test-operation", TestOperation);

console.log('Finish executing customizable_module.js')
