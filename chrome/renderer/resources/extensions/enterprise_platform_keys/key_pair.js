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
 * Implementation of WebCrypto.KeyPair used in enterprise.platformKeys.
 * @param {ArrayBuffer} publicKeySpki The Subject Public Key Info in DER
 *   encoding.
 * @param {KeyAlgorithm} algorithm The algorithm identifier.
 * @param {KeyUsage[]} usages The allowed key usages.
 * @constructor
 */
function KeyPairImpl(publicKeySpki, algorithm, usages) {
  this.publicKey = new Key(KeyType.public,
                           publicKeySpki,
                           algorithm,
                           intersect([KeyUsage.verify], usages),
                           true /* extractable */);
  this.privateKey = new Key(KeyType.private,
                            publicKeySpki,
                            algorithm,
                            intersect([KeyUsage.sign], usages),
                            false /* not extractable */);
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

exports.$set('KeyPair', KeyPair);
