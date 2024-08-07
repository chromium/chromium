// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  // clang-format off
  'ignorePatterns' : ['chai_v4.js'],
  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      // Turn off until all violations under this folder are fixed. This was
      // done for other parts of the codebase in http://crbug.com/1521107
      'no-restricted-syntax': 'off',

      // Turn off until all violations under this folder are fixed. This was
      // done for other parts of the codebase in http://crbug.com/1494527
      '@typescript-eslint/consistent-type-imports' : 'off',
    },
  }],
  // clang-format on
};
