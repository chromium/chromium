// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var internalAPI = getInternalApi('enterprise.platformKeysInternal');
var intersect = require('platformKeys.utils').intersect;
var subtleCryptoModule = require('platformKeys.SubtleCrypto');
var SubtleCryptoImpl = subtleCryptoModule.SubtleCryptoImpl;
var KeyPair = require('enterprise.platformKeys.KeyPair').KeyPair;
var KeyUsage = require('platformKeys.Key').KeyUsage;

var normalizeAlgorithm =
    requireNative('platform_keys_natives').NormalizeAlgorithm;

// This error is thrown by the internal and public API's token functions and
// must be rethrown by this custom binding. Keep this in sync with the C++ part
// of this API.
var errorInvalidToken = 'The token is not valid.';

// The following errors are specified in WebCrypto.
// TODO(pneubeck): These should be DOMExceptions.
function CreateNotSupportedError() {
  return new Error('The algorithm is not supported');
}

function CreateInvalidAccessError() {
  return new Error('The requested operation is not valid for the provided key');
}

function CreateDataError() {
  return new Error('Data provided to an operation does not meet requirements');
}

function CreateSyntaxError() {
  return new Error('A required parameter was missing or out-of-range');
}

function CreateOperationError() {
  return new Error('The operation failed for an operation-specific reason');
}

// Catches an |internalErrorInvalidToken|. If so, forwards it to |reject| and
// returns true.
function catchInvalidTokenError(reject) {
  if (chrome.runtime.lastError &&
      chrome.runtime.lastError.message === errorInvalidToken) {
    reject(chrome.runtime.lastError);
    return true;
  }
  return false;
}

// Returns true if the |normalizedAlgorithm| returned by normalizeAlgorithm() is
// supported by platform keys subtle crypto internal API.
function isSupportedGenerateKeyAlgorithm(normalizedAlgorithm) {
  if (normalizedAlgorithm.name === 'RSASSA-PKCS1-v1_5') {
    return equalsStandardPublicExponent(normalizedAlgorithm.publicExponent);
  }

  if (normalizedAlgorithm.name === 'ECDSA') {
    // Only NIST P-256 curve is supported.
    return normalizedAlgorithm.namedCurve === 'P-256';
  }

  return false;
}

// Returns true if |array| is a BigInteger describing the standard public
// exponent 65537. In particular, it ignores leading zeros as required by the
// BigInteger definition in WebCrypto.
function equalsStandardPublicExponent(array) {
  var expected = [0x01, 0x00, 0x01];
  if (array.length < expected.length)
    return false;
  for (var i = 0; i < array.length; i++) {
    var expectedDigit = 0;
    if (i < expected.length) {
      // |expected| is symmetric, endianness doesn't matter.
      expectedDigit = expected[i];
    }
    if (array[array.length - 1 - i] !== expectedDigit)
      return false;
  }
  return true;
}

/**
 * Implementation of WebCrypto.SubtleCrypto used in enterprise.platformKeys.
 * Derived from platformKeys.SubtleCrypto.
 * @param {string} tokenId The id of the backing Token.
 * @param {boolean} softwareBacked Whether the key operations should be executed
 *     in software.
 * @constructor
 */
function EnterpriseSubtleCryptoImpl(tokenId, softwareBacked) {
  $Function.call(SubtleCryptoImpl, this, tokenId, softwareBacked);
}

EnterpriseSubtleCryptoImpl.prototype =
    $Object.create(SubtleCryptoImpl.prototype);

EnterpriseSubtleCryptoImpl.prototype.generateKey =
    function(algorithm, extractable, keyUsages) {
  var subtleCrypto = this;
  return new Promise(function(resolve, reject) {
    // TODO(pneubeck): Apply the algorithm normalization of the WebCrypto
    // implementation.

    if (extractable) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }
    if (intersect(keyUsages, [KeyUsage.sign, KeyUsage.verify]).length !=
        keyUsages.length) {
      throw CreateDataError();
    }
    var normalizedAlgorithmParameters =
        normalizeAlgorithm(algorithm, 'GenerateKey');
    if (!normalizedAlgorithmParameters) {
      // TODO(pneubeck): It's not clear from the WebCrypto spec which error to
      // throw here.
      throw CreateSyntaxError();
    }

    if (!isSupportedGenerateKeyAlgorithm(normalizedAlgorithmParameters)) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }

    if (normalizedAlgorithmParameters.name === 'RSASSA-PKCS1-v1_5') {
      // normalizeAlgorithm returns an array, but publicExponent should be a
      // Uint8Array.
      normalizedAlgorithmParameters.publicExponent =
          new Uint8Array(normalizedAlgorithmParameters.publicExponent);
    }

    internalAPI.generateKey(
        subtleCrypto.tokenId, normalizedAlgorithmParameters,
        subtleCrypto.softwareBacked, function(spki) {
          if (catchInvalidTokenError(reject))
            return;
          if (chrome.runtime.lastError) {
            reject(CreateOperationError());
            return;
          }
          resolve(new KeyPair(spki, normalizedAlgorithmParameters, keyUsages));
        });
  });
};

function SubtleCrypto() {
  privates(SubtleCrypto).constructPrivate(this, arguments);
}
utils.expose(SubtleCrypto, EnterpriseSubtleCryptoImpl, {
  superclass: subtleCryptoModule.SubtleCrypto,
  functions: [
    'generateKey',
    // 'sign', 'exportKey' are exposed by the base class
  ],
});

exports.$set('SubtleCrypto', SubtleCrypto);
