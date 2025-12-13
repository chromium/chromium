// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const utils = require('utils');
const intersect = require('platformKeys.utils').intersect;
const keyModule = require('platformKeys.Key');
const Key = keyModule.Key;
const KeyType = keyModule.KeyType;
const KeyUsage = keyModule.KeyUsage;

/**
 * Implementation of WebCrypto.CryptoKeyPair used in enterprise.platformKeys.
 * @param {ArrayBuffer} keyIdentifier The key identifier. For asymmetric keys,
 *     it corresponds to the Subject Public Key Info (SPKI) in DER encoding.
 * @param {KeyAlgorithm} algorithm The algorithm identifier.
 * @param {KeyUsage[]} usages The allowed key usages.
 * @constructor
 */
function KeyPairImpl(keyIdentifier, algorithm, usages) {
  const allowedPublicKeyAlgorithms = [KeyUsage.verify];
  const allowedPrivateKeyAlgorithms = [KeyUsage.sign, KeyUsage.unwrapKey];

  this.publicKey = new Key(
      KeyType.public, keyIdentifier, algorithm,
      intersect(allowedPublicKeyAlgorithms, usages), /*extractable=*/ true);
  this.privateKey = new Key(
      KeyType.private, keyIdentifier, algorithm,
      intersect(allowedPrivateKeyAlgorithms, usages), /*extractable=*/ false);
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
