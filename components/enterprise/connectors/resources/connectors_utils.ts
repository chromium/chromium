// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DictionaryValue, Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import type {Int32Value} from './connectors_internals.mojom-webui.js';
import {KeyTrustLevel, KeyType} from './connectors_internals.mojom-webui.js';

const TrustLevelStringMap = {
  [KeyTrustLevel.UNSPECIFIED]: 'Unspecified',
  [KeyTrustLevel.HW]: 'HW',
  [KeyTrustLevel.OS]: 'OS',
  [KeyTrustLevel.OS_SOFTWARE]: 'OS Storage',
};

const KeyTypeStringMap = {
  [KeyType.UNKNOWN]: 'Unknown',
  [KeyType.RSA]: 'RSA',
  [KeyType.EC]: 'EC',
};

export function trustLevelToString(trustLevel: KeyTrustLevel): string {
  return TrustLevelStringMap[trustLevel] || 'invalid';
}

export function keyTypeToString(keyType: KeyType): string {
  return KeyTypeStringMap[keyType] || 'invalid';
}

export function keySyncCodeToString(
    syncKeyResponseCode: (Int32Value|null|undefined)): string {
  if (!syncKeyResponseCode) {
    return 'Undefined';
  }

  const value = syncKeyResponseCode.value;
  if (value / 100 === 2) {
    return `Success (${value})`;
  }
  return `Failure (${value})`;
}

// Converts a `mojo_base.mojom.Value` union (e.g. `{boolValue: true}`,
// `{listValue: {storage: [...]}}`) into the equivalent plain JS value.
function mojoValueToJs(value: Value): unknown {
  if (value.nullValue !== undefined) {
    return null;
  }
  if (value.boolValue !== undefined) {
    return value.boolValue;
  }
  if (value.intValue !== undefined) {
    return value.intValue;
  }
  if (value.doubleValue !== undefined) {
    return value.doubleValue;
  }
  if (value.stringValue !== undefined) {
    return value.stringValue;
  }
  if (value.binaryValue !== undefined) {
    return value.binaryValue;
  }
  if (value.listValue !== undefined) {
    return value.listValue.storage.map(mojoValueToJs);
  }
  if (value.dictionaryValue !== undefined) {
    return mojoDictionaryToJs(value.dictionaryValue);
  }
  return undefined;
}

function mojoDictionaryToJs(dictionary: DictionaryValue):
    Record<string, unknown> {
  return Object.fromEntries(
      Object.entries(dictionary.storage)
          .map(([key, item]) => [key, mojoValueToJs(item)]));
}

// Returns a readable, pretty-printed JSON string for `dictionary`.
export function mojoDictionaryToJsonString(dictionary: DictionaryValue):
    string {
  return JSON.stringify(mojoDictionaryToJs(dictionary), null, 2);
}
