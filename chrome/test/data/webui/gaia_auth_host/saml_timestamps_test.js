// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decodeTimestamp} from 'chrome://chrome-signin/gaia_auth_host/saml_timestamps.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

const ROUNDTRIP_DATA = [
  '1980-01-01',
  '1990-02-12',
  '2000-03-13',
  '2010-04-14',
  '2020-05-25',
  '2050-06-26',
  '2100-07-27',
  '3000-08-30',
  '9999-12-31',
];

const NTFS_EPOCH = new Date('1601-01-01 UTC');

function assertDecodesAs(expectedStr, encodedStr) {
  const expectedMs = Date.parse(expectedStr);
  const decodedMs = decodeTimestamp(encodedStr).valueOf();
  assertEquals(
      expectedMs, decodedMs,
      `Expected "${encodedStr}" to be decoded as "${expectedStr}"`);
}

function assertInvalid(encoded) {
  assertEquals(
      null, decodeTimestamp(encoded),
      `Expected that "${encoded}" would not decode since it is invalid`);
}

suite('SamlTimestampsSuite', function() {
  test('DecodeValidExamples', () => {
    assertDecodesAs('2004-09-16T00:00Z', '1095292800');     // Unix time (s)
    assertDecodesAs('2004-09-16T00:00Z', '1095292800000');  // Unix time (ms)

    assertDecodesAs(
        '2015-08-30T00:00Z', '130853664000000000');  // NTFS filetime

    // ISO 8601
    assertDecodesAs('2020-06-04T00:00Z', '2020-06-04');
    assertDecodesAs('2020-06-04T12:00Z', '2020-06-04T12:00');
    assertDecodesAs('2020-06-04T18:00Z', '2020-06-04T18:00Z');
  });

  test('RoundtripAsNTFS', () => {
    for (const data of ROUNDTRIP_DATA) {
      const ntfsTicks = (Date.parse(data) - NTFS_EPOCH.valueOf()) * 10000;
      assertDecodesAs(data, ntfsTicks.toString());
    }
  });

  test('RoundtripAsUnixSeconds', () => {
    for (const data of ROUNDTRIP_DATA) {
      const unixSeconds = Date.parse(data) / 1000;
      assertDecodesAs(data, unixSeconds.toString());
    }
  });

  test('RoundtripAsUnixMilliseconds', () => {
    for (const data of ROUNDTRIP_DATA) {
      const unixMilliseconds = Date.parse(data);
      assertDecodesAs(data, unixMilliseconds.toString());
    }
  });

  test('RoundtripAsIso8601', () => {
    for (const data of ROUNDTRIP_DATA) {
      assertDecodesAs(data, data);
    }
  });

  test('Iso8601Timezones', () => {
    // No time specified decodes as midnight UTC
    assertDecodesAs('2020-06-04T00:00Z', '2020-06-04');
    // No timezone specified decodes as UTC
    assertDecodesAs('2020-06-04T12:00Z', '2020-06-04T12:00');

    // There are different ways of specifying UTC
    assertDecodesAs('2020-06-04T18:00Z', '2020-06-04T18:00Z');
    assertDecodesAs('2020-06-04T18:00Z', '2020-06-04T18:00+0000');
    assertDecodesAs('2020-06-04T18:00Z', '2020-06-04T18:00+00:00');

    // Other timezones can also be specified
    assertDecodesAs('2020-06-04T11:00Z', '2020-06-04T12:00+0100');
    assertDecodesAs('2020-06-04T11:00Z', '2020-06-04T12:00+01:00');
    assertDecodesAs('2020-06-04T13:00Z', '2020-06-04T12:00-0100');
    assertDecodesAs('2020-06-04T13:00Z', '2020-06-04T12:00-01:00');
  });

  test('DecodeSpecialIntegers', () => {
    // Zero or -1 could mean 1970 or 1601 - we can't guarantee we will decode it
    // to the exact right value, but we must make sure it means the distant
    // past.
    assertTrue(decodeTimestamp('0') < new Date('2000-01-01'));
    assertTrue(decodeTimestamp('-1') < new Date('2000-01-01'));

    // Max signed int32 (2^31 - 1). Should decode as the year 2038:
    assertDecodesAs('2038-01-19T03:14:07Z', '2147483647');

    // Max signed int64 (2^63 - 1). Should decode to sometime in the very
    // distant future, but it doesn't matter exactly when:
    assertTrue(decodeTimestamp('9223372036854775807') > new Date('3000-01-01'));
  });

  test('DecodeInvalid', () => {
    // Try some junk:
    assertInvalid('');
    assertInvalid('abc');
    assertInvalid('01-02');

    // These look like dates, but are not ISO 8601 so are rejected.
    // For dates that are not in ISO 8601, we can't reliably know what was
    // meant.
    assertInvalid('01-02-03');
    assertInvalid('99-01-01');
    assertInvalid('01-02-2003');
    assertInvalid('2020.01.01');
    assertInvalid('2020/01/01');
    assertInvalid('01 January 2020');
    assertInvalid('2020-31-01');        // Almost, but there's no 31st month.
    assertInvalid('2020-02-01 02:15');  // Almost, but missing the T separator
    // Almost, but timezone must be described explicitly as +100:
    assertInvalid('2020-02-01T02:15 (Central European Time)');
  });
});
