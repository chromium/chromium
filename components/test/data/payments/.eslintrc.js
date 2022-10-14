// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env': {
    'browser': true,
    'es6': true,
  },
  'rules': {
    'no-var': 'off',
    'prefer-const': 'off',
    'no-restricted-properties': 0,
    'no-irregular-whitespace': 2,
    'no-unexpected-multiline': 2,
    'valid-jsdoc': [
      2,
      {
        requireParamDescription: true,
        requireReturnDescription: true,
        requireReturn: false,
        prefer: {
          returns: 'return',
        },
      },
    ],
    'curly': [2, 'multi-line'],
    'guard-for-in': 2,
    'no-caller': 2,
    'no-extend-native': 2,
    'no-extra-bind': 2,
    'no-invalid-this': 2,
    'no-multi-spaces': 2,
    'no-multi-str': 2,
    'no-new-wrappers': 2,
    'no-throw-literal': 2,
    'no-with': 2,
    'array-bracket-spacing': [2, 'never'],
    'brace-style': 2,
    'camelcase': [
      2,
      {
        properties: 'never',
      },
    ],
    'comma-dangle': [2, 'always-multiline'],
    'comma-spacing': 2,
    'comma-style': 2,
    'computed-property-spacing': 2,
    'eol-last': 2,
    'func-call-spacing': 2,
    'key-spacing': 2,
    'keyword-spacing': 2,
    'linebreak-style': 2,
    'max-len': [
      2,
      {
        code: 80,
        tabWidth: 2,
        ignoreUrls: true,
      },
    ],
    'new-cap': 2,
    'no-array-constructor': 2,
    'no-mixed-spaces-and-tabs': 2,
    'no-multiple-empty-lines': [
      2,
      {
        max: 2,
      },
    ],
    'no-new-object': 2,
    'no-trailing-spaces': 2,
    'object-curly-spacing': 2,
    'padded-blocks': [2, 'never'],
    'quote-props': [2, 'consistent'],
    'quotes': [
      2,
      'single',
      {
        allowTemplateLiterals: true,
      },
    ],
    'require-jsdoc': [
      2,
      {
        require: {
          FunctionDeclaration: true,
          MethodDefinition: true,
          ClassDeclaration: true,
        },
      },
    ],
    'semi-spacing': 2,
    'semi': 2,
    'space-before-blocks': 2,
    'space-before-function-paren': [2, 'never'],
    'spaced-comment': [2, 'always'],
    'arrow-parens': [2, 'always'],
    'constructor-super': 2,
    'generator-star-spacing': [2, 'after'],
    'no-new-symbol': 2,
    'no-this-before-super': 2,
    'prefer-rest-params': 2,
    'prefer-spread': 2,
    'rest-spread-spacing': 2,
    'yield-star-spacing': [2, 'after'],
  },
};
