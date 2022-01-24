// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';

import {assertThrows} from '../../chai_assert.js';

const mojom = chromeos.networkConfig.mojom;

suite('OncMojoTest', () => {
  test('Serialize Domain Suffix Match', () => {
    expectEquals(OncMojo.serializeDomainSuffixMatch([]), '');
    expectEquals(
        OncMojo.serializeDomainSuffixMatch(['example.com']), 'example.com');
    expectEquals(
        OncMojo.serializeDomainSuffixMatch(
            ['example1.com', 'example2.com', 'example3.com']),
        'example1.com;example2.com;example3.com');
  });

  test('Deserialize Domain Suffix Match', () => {
    const expectEqualValues = (serializedVal, val) => {
      JSON.stringify(OncMojo.deserializeDomainSuffixMatch(serializedVal)),
          JSON.stringify([val]);
    };
    expectEqualValues('', []);
    expectEqualValues('  ', []);
    expectEqualValues('example', ['example']);
    expectEqualValues('example;', ['example']);
    expectEqualValues('example1;example2', ['example1', 'example2']);
    // '#' is a non-RFC compliant DNS character.
    expectEquals(OncMojo.deserializeDomainSuffixMatch('example#'), null);
  });

  test('Serialize Subject Alternative Name Match', () => {
    expectEquals(OncMojo.serializeSubjectAltNameMatch([]), '');
    expectEquals(
        OncMojo.serializeSubjectAltNameMatch([
          {type: mojom.SubjectAltName_Type.kEmail, value: 'test@example.com'},
          {type: mojom.SubjectAltName_Type.kUri, value: 'http://test.com'}
        ]),
        'EMAIL:test@example.com;URI:http://test.com');
  });

  test('Deserialize Subject Alternative Name Match', () => {
    const expectEqualValues = (serializedVal, val) => {
      JSON.stringify(OncMojo.deserializeSubjectAltNameMatch(serializedVal)),
          JSON.stringify([val]);
    };
    expectEqualValues('', []);
    expectEqualValues('  ', []);
    expectEqualValues('EMAIL:test@example.com;uri:http://test.com', [
      {type: mojom.SubjectAltName_Type.kEmail, value: 'test@example.com'},
      {type: mojom.SubjectAltName_Type.kUri, value: 'http://test.com'}
    ]);
    // Malformed SAN entry.
    expectEquals(
        OncMojo.deserializeSubjectAltNameMatch('EMAILtest@example.com'), null);
    // Incorrect SAN type.
    expectEquals(
        OncMojo.deserializeSubjectAltNameMatch('E:test@example.com'), null);
    // Non-RFC compliant character.
    expectEquals(
        OncMojo.deserializeSubjectAltNameMatch('EMAIL:test@exa\'mple.com'),
        null);
  });
});
