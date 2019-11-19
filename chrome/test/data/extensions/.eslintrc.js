// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We have a relaxed version of the eslintrc file for
// //chrome/test/data/extensions because it's massively out-of-sync with our
// JS style guide. See https://crbug.com/941239.
module.exports = {
  'root': true,
  'env': {
    'browser': true,
    'es6': true,
  },
  'parserOptions': {
    'ecmaVersion': 2017,
  },
  'rules': {
    // Enabled checks.
    'no-extra-semi': 'error',
    'semi': ['error', 'always'],
  },
};
