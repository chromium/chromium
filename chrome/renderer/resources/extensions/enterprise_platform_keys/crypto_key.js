// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var intersect = require('platformKeys.utils').intersect;
var keyModule = require('platformKeys.Key');
var Key = keyModule.Key;
var KeyType = keyModule.KeyType;
var KeyUsage = keyModule.KeyUsage;

/**
 * Implementation of WebCrypto.CryptoKeyPair used in enterprise.platformKeys.
 * @param {ArrayBuffer} keyIdentifier The key identifier. For asymmetric keys,
 *     it corresponds to the Subject Public Key Info (SPKI) in DER encoding.
 * @param {KeyAlgorithm} algorithm The algorithm identifier.
 * @param {KeyUsage[]} usages The allowed key usages.
 * @constructor
 */
function KeyPairImpl(keyIdentifier, algorithm, usages) {
  this.publicKey = new Key(
      KeyType.public, keyIdentifier, algorithm,
      intersect([KeyUsage.verify], usages), /*extractable=*/ true);
  this.privateKey = new Key(
      KeyType.private, keyIdentifier, algorithm,
      intersect([KeyUsage.sign], usages), /*extractable=*/ false);
}
$Object.setPrototypeOf(KeyPairImpl.prototype, null);

function KeyPair() {
  privates(KeyPair).constructPrivate(this, arguments);
}
utils.expose(KeyPair, KeyPairImpl, {
  readonly: [
    'publicKey',
    'privateKey',
  ],
});

/**
 * Implementation of WebCrypto.CryptoKey used in enterprise.platformKeys.
 * @param {ArrayBuffer} keyIdentifier The key identifier. For symmetric keys,
 *     it corresponds to the internally generated symKeyId.
 * @param {KeyAlgorithm} algorithm The algorithm identifier.
 * @param {KeyUsage[]} usages The allowed key usages.
 * @constructor
 */
function SymKeyImpl(keyIdentifier, algorithm, usages) {
  this.key = new Key(
      KeyType.secret, keyIdentifier, algorithm, usages, /*extractable=*/ false);
}
$Object.setPrototypeOf(SymKeyImpl.prototype, null);

function SymKey() {
  privates(SymKey).constructPrivate(this, arguments);
}
utils.expose(SymKey, SymKeyImpl, {
  readonly: [
    'key',
  ],
});

exports.$set('KeyPair', KeyPair);
exports.$set('SymKey', SymKey);
