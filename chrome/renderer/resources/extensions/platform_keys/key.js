// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');

/**
 * Enum of possible key types (subset of WebCrypto.KeyType).
 * @enum {string}
 */
var KeyType = {
  __proto__: null,
  public: 'public',
  private: 'private'
};

/**
 * Enum of possible key usages (subset of WebCrypto.KeyUsage).
 * @enum {string}
 */
var KeyUsage = {
  __proto__: null,
  sign: 'sign',
  verify: 'verify'
};

/**
 * Implementation of WebCrypto.Key used in enterprise.platformKeys.
 * @param {KeyType} type The type of the new key.
 * @param {ArrayBuffer} publicKeySpki The Subject Public Key Info in DER
 *   encoding.
 * @param {KeyAlgorithm} algorithm The algorithm identifier.
 * @param {KeyUsage[]} usages The allowed key usages.
 * @param {boolean} extractable Whether the key is extractable.
 * @constructor
 */
function KeyImpl(type, publicKeySpki, algorithm, usages, extractable) {
  this.type = type;
  this.spki = publicKeySpki;
  this.algorithm = algorithm;
  this.usages = usages;
  this.extractable = extractable;
}
$Object.setPrototypeOf(KeyImpl.prototype, null);

/**
 * The public base class of Key.
 */
function KeyBase() {}
KeyBase.prototype = {
  constructor: KeyBase,
  get algorithm() {
    return utils.deepCopy(privates(this).impl.algorithm);
  },
};

function Key() {
  privates(Key).constructPrivate(this, arguments);
}
utils.expose(Key, KeyImpl, {
  superclass: KeyBase,
  readonly: [
    'extractable',
    'type',
    'usages',
  ],
});

/**
 * Returns |key|'s Subject Public Key Info. Throws an exception if |key| is not
 * a valid Key object.
 * @param {Key} key
 * @return {ArrayBuffer} The Subject Public Key Info in DER encoding of |key|.
 */
function getSpki(key) {
  if (!privates(key))
    throw new Error('Invalid key object.');
  var keyImpl = privates(key).impl;
  if (!keyImpl || !keyImpl.spki)
    throw new Error('Invalid key object.');
  return keyImpl.spki;
}

exports.$set('Key', Key);
exports.$set('KeyType', KeyType);
exports.$set('KeyUsage', KeyUsage);
exports.$set('getSpki', getSpki);
