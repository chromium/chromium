// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var internalAPI = getInternalApi('enterprise.platformKeysInternal');
var intersect = require('platformKeys.utils').intersect;
var subtleCryptoModule = require('platformKeys.SubtleCrypto');
var SubtleCryptoImpl = subtleCryptoModule.SubtleCryptoImpl;
var catchInvalidTokenError = subtleCryptoModule.catchInvalidTokenError;
var KeyPair = require('enterprise.platformKeys.CryptoKey').KeyPair;
var SymKey = require('enterprise.platformKeys.CryptoKey').SymKey;
var KeyUsage = require('platformKeys.Key').KeyUsage;

var normalizeAlgorithm =
    requireNative('platform_keys_natives').NormalizeAlgorithm;

// The following errors are specified in WebCrypto.
// TODO(pneubeck): These should be DOMExceptions.
function CreateNotSupportedError() {
  return new Error('The algorithm is not supported');
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

// Checks if the given algorithm has the expected RSA name.
function isSupportedRsaAlgorithmName(normalizedAlgorithmParams) {
  return normalizedAlgorithmParams.name === 'RSASSA-PKCS1-v1_5';
}

// Checks if the given algorithm has the expected EC name.
function isSupportedEcAlgorithmName(normalizedAlgorithmParams) {
  return normalizedAlgorithmParams.name === 'ECDSA';
}

// Checks if the given algorithm has the expected AES name.
function isSupportedAesAlgorithmName(normalizedAlgorithmParams) {
  return normalizedAlgorithmParams.name === 'AES-CBC';
}

// Returns true if the `normalizedAlgorithmParams` returned by
// normalizeAlgorithm() is supported by platform keys subtle crypto internal
// API.
function isSupportedGenerateKeyAlgorithm(normalizedAlgorithmParams) {
  if (isSupportedRsaAlgorithmName(normalizedAlgorithmParams)) {
    return equalsStandardPublicExponent(
        normalizedAlgorithmParams.publicExponent);
  }

  if (isSupportedEcAlgorithmName(normalizedAlgorithmParams)) {
    // Only NIST P-256 curve is supported.
    return normalizedAlgorithmParams.namedCurve === 'P-256';
  }

  if (isSupportedAesAlgorithmName(normalizedAlgorithmParams)) {
    // AES keys are only supported with 256 bits.
    return normalizedAlgorithmParams.length === 256;
  }

  return false;
}

// Returns true if `array` is a BigInteger describing the standard public
// exponent 65537. In particular, it ignores leading zeros as required by the
// BigInteger definition in WebCrypto.
function equalsStandardPublicExponent(array) {
  var expected = [0x01, 0x00, 0x01];
  if (array.length < expected.length) {
    return false;
  }
  for (var i = 0; i < array.length; i++) {
    var expectedDigit = 0;
    if (i < expected.length) {
      // `expected` is symmetric, endianness doesn't matter.
      expectedDigit = expected[i];
    }
    if (array[array.length - 1 - i] !== expectedDigit) {
      return false;
    }
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
    var normalizedAlgorithmParams =
        normalizeAlgorithm(algorithm, 'GenerateKey');
    if (!normalizedAlgorithmParams) {
      // TODO(pneubeck): It's not clear from the WebCrypto spec which error to
      // throw here.
      throw CreateSyntaxError();
    }

    if (!isSupportedGenerateKeyAlgorithm(normalizedAlgorithmParams)) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }

    if (isSupportedRsaAlgorithmName(normalizedAlgorithmParams)) {
      // `normalizeAlgorithm` returns an ArrayBuffer, but publicExponent should
      // be a Uint8Array.
      normalizedAlgorithmParams.publicExponent =
          new Uint8Array(normalizedAlgorithmParams.publicExponent);
    }

    internalAPI.generateKey(
        subtleCrypto.tokenId, normalizedAlgorithmParams,
        subtleCrypto.softwareBacked, function(identifier) {
          if (catchInvalidTokenError(reject)) {
            return;
          }
          if (chrome.runtime.lastError) {
            reject(CreateOperationError());
            return;
          }

          if (isSupportedAesAlgorithmName(normalizedAlgorithmParams)) {
            resolve(
                new SymKey(identifier, normalizedAlgorithmParams, keyUsages));
            return;
          }

          if (isSupportedRsaAlgorithmName(normalizedAlgorithmParams) ||
              isSupportedEcAlgorithmName(normalizedAlgorithmParams)) {
            resolve(
                new KeyPair(identifier, normalizedAlgorithmParams, keyUsages));
            return;
          }

          // This code should not be reached, unless we change the list of
          // supported algorithms in `isSupportedGenerateKeyAlgorithm()` and
          // forget to update one of the conditions above.
          reject(CreateNotSupportedError());
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
    // 'sign' and 'exportKey' are exposed by the base class.
  ],
});

exports.$set('SubtleCrypto', SubtleCrypto);
