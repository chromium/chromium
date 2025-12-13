// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const utils = require('utils');
const internalAPI = getInternalApi('enterprise.platformKeysInternal');
const intersect = require('platformKeys.utils').intersect;
const subtleCryptoModule = require('platformKeys.SubtleCrypto');
const SubtleCryptoImpl = subtleCryptoModule.SubtleCryptoImpl;
const catchInvalidTokenError = subtleCryptoModule.catchInvalidTokenError;
const KeyPair = require('enterprise.platformKeys.CryptoKey').KeyPair;
const SymKey = require('enterprise.platformKeys.CryptoKey').SymKey;
const KeyUsage = require('platformKeys.Key').KeyUsage;

const normalizeAlgorithm =
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

// Checks if the given algorithm name corresponds to one of the RSA algorithms
// supported by this API.
function isSupportedRsaAlgorithmName(algorithmParams) {
  return algorithmParams.name === 'RSASSA-PKCS1-v1_5' ||
      algorithmParams.name === 'RSA-OAEP';
}

// Checks if the given algorithm has the expected EC name.
function isSupportedEcAlgorithmName(algorithmParams) {
  return algorithmParams.name === 'ECDSA';
}

// Checks if the given algorithm has the expected AES name.
function isSupportedAesAlgorithmName(algorithmParams) {
  return algorithmParams.name === 'AES-CBC';
}

// Returns true if the `algorithmParams` returned by normalizeAlgorithm() is
// supported by platform keys subtle crypto internal API.
function isSupportedGenerateKeyAlgorithm(algorithmParams) {
  if (isSupportedRsaAlgorithmName(algorithmParams)) {
    return equalsStandardPublicExponent(algorithmParams.publicExponent);
  }

  if (isSupportedEcAlgorithmName(algorithmParams)) {
    // Only NIST P-256 curve is supported.
    return algorithmParams.namedCurve === 'P-256';
  }

  if (isSupportedAesAlgorithmName(algorithmParams)) {
    // AES keys are only supported with 256 bits.
    return algorithmParams.length === 256;
  }

  return false;
}

// Returns true if `array` is a BigInteger describing the standard public
// exponent 65537. In particular, it ignores leading zeros as required by the
// BigInteger definition in WebCrypto.
function equalsStandardPublicExponent(array) {
  const expected = [0x01, 0x00, 0x01];
  if (array.length < expected.length) {
    return false;
  }
  for (let i = 0; i < array.length; i++) {
    let expectedDigit = 0;
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

// Validates that the `keyUsages` list only contains operations allowed by the
// platformKeys API for the given key algorithm.
function validateKeyUsageRestrictions(algorithmName, keyUsages) {
  if (algorithmName === 'RSASSA-PKCS1-v1_5' || algorithmName === 'ECDSA') {
    const filteredKeyUsages =
        intersect(keyUsages, [KeyUsage.sign, KeyUsage.verify]);
    return filteredKeyUsages.length === keyUsages.length;
  }

  if (algorithmName === 'RSA-OAEP') {
    const filteredKeyUsages = intersect(keyUsages, [KeyUsage.unwrapKey]);
    return filteredKeyUsages.length === keyUsages.length;
  }

  if (algorithmName === 'AES-CBC') {
    // TODO(crbug.com/325011140): Update the condition below to validate the
    // `keyUsages` against the allowed usages for AES-CBC keys, when those keys
    // are fully supported.
    return keyUsages.length === 0;
  }

  // This code should not be reached, unless we change the list of supported
  // algorithms in `isSupportedGenerateKeyAlgorithm()` and forget to update one
  // of the conditions above.
  return false;
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

EnterpriseSubtleCryptoImpl.prototype.generateKey = function(
    algorithm, extractable, keyUsages) {
  const subtleCrypto = this;
  return new Promise(function(resolve, reject) {
    // TODO(pneubeck): Apply the algorithm normalization of the WebCrypto
    // implementation.

    if (extractable) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }
    const allowedKeyUsages =
        [KeyUsage.sign, KeyUsage.verify, KeyUsage.unwrapKey];
    if (intersect(keyUsages, allowedKeyUsages).length !== keyUsages.length) {
      throw CreateDataError();
    }
    const normalizedAlgorithmParams =
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

    if (!validateKeyUsageRestrictions(
            normalizedAlgorithmParams.name, keyUsages)) {
      throw CreateDataError();
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
