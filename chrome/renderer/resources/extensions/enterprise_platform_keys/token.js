// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var SubtleCrypto = require('enterprise.platformKeys.SubtleCrypto').SubtleCrypto;

/**
 * Implementation of enterprise.platformKeys.Token.
 * @param {string} id The id of the new Token.
 * @constructor
 */
function TokenImpl(id) {
  this.id = id;
  this.subtleCrypto = new SubtleCrypto(id, /*softwareBacked=*/ false);
  this.softwareBackedSubtleCrypto =
      new SubtleCrypto(id, /*softwareBacked=*/ true);
}
$Object.setPrototypeOf(TokenImpl.prototype, null);

function Token() {
  privates(Token).constructPrivate(this, arguments);
}
utils.expose(Token, TokenImpl, {
  readonly: [
    'id',
    'subtleCrypto',
    'softwareBackedSubtleCrypto',
  ],
});

exports.$set('Token', Token);
