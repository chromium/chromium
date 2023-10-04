// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env': {'browser': true, 'es6': true},
  'rules': {
    'no-restricted-properties': 'off',
    'eqeqeq': ['error', 'always', {'null': 'ignore'}],
  },

  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      '@typescript-eslint/naming-convention': [
        'error',
        // Override default format to allow test functions like testFoo_bar().
        {
          selector: 'function',
          format: null,
        },
      ],
    },
  }],
};
