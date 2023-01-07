// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env': {'browser': true, 'es6': true},
  'rules': {
    'no-restricted-properties': [
      'error', {
        'object': 'MockInteractions',
        'property': 'tap',
        'message': 'Do not use on-tap handlers in prod code, and use the ' +
            'native click() method in tests. See more context at ' +
            'crbug.com/812035.',
      },
      {
        'object': 'test',
        'property': 'only',
        'message': 'test.only() silently disables other tests in the same ' +
            'suite(). Did you forget deleting it before uploading? Use ' +
            'test.skip() instead to explicitly disable certain test() cases.',
      },
    ],
    'eqeqeq': ['error', 'always', {'null': 'ignore'}],
  },
};
