// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var internalAPI = getInternalApi('platformKeysInternal');
var keyModule = require('platformKeys.Key');
var getKeyIdentifier = keyModule.getKeyIdentifier;
var KeyUsage = keyModule.KeyUsage;

var normalizeAlgorithm =
    requireNative('platform_keys_natives').NormalizeAlgorithm;

// This error is thrown by the internal and public API's token functions and
// must be re-thrown by this custom binding. Keep this in sync with the C++ part
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

function CreateSyntaxError() {
  return new Error('A required parameter was missing or out-of-range');
}

function CreateOperationError() {
  return new Error('The operation failed for an operation-specific reason');
}

// Catches an |internalErrorInvalidToken|. If so, forwards it to |reject| and
// returns true.
function catchInvalidTokenError(reject) {
  if (bindingUtil.hasLastError() &&
      bindingUtil.getLastErrorMessage() === errorInvalidToken) {
    var error = chrome.runtime.lastError;
    bindingUtil.clearLastError();
    reject(error);
    return true;
  }
  return false;
}

// Returns true if the |normalizedAlgorithm| returned by normalizeAlgorithm() is
// supported by platform keys subtle crypto internal API.
function isSupportedSignAlgorithm(normalizedAlgorithm) {
  if (normalizedAlgorithm.name === 'RSASSA-PKCS1-v1_5') {
    return true;
  }

  if (normalizedAlgorithm.name === 'ECDSA') {
    // Only SHA-256 algorithm is supported for ECDSA.
    return normalizedAlgorithm.hash.name === 'SHA-256';
  }

  return false;
}

/**
 * Implementation of WebCrypto.SubtleCrypto used in platformKeys and
 * enterprise.platformKeys.
 * @param {string} tokenId The id of the backing Token.
 * @param {boolean} softwareBacked Whether the key operations should be executed
 *     in software.
 * @constructor
 */
function SubtleCryptoImpl(tokenId, softwareBacked) {
  this.tokenId = tokenId;
  this.softwareBacked = softwareBacked;
}
$Object.setPrototypeOf(SubtleCryptoImpl.prototype, null);

SubtleCryptoImpl.prototype.sign = function(algorithm, key, dataView) {
  var subtleCrypto = this;
  return new Promise(function(resolve, reject) {
    if (key.type !== 'private' || key.usages.indexOf(KeyUsage.sign) === -1) {
      throw CreateInvalidAccessError();
    }

    var normalizedAlgorithmParameters = normalizeAlgorithm(algorithm, 'Sign');
    if (!normalizedAlgorithmParameters) {
      // TODO(pneubeck): It's not clear from the WebCrypto spec which error to
      // throw here.
      throw CreateSyntaxError();
    }

    if (normalizedAlgorithmParameters.name !== key.algorithm.name) {
      throw CreateInvalidAccessError();
    }

    if (!isSupportedSignAlgorithm(normalizedAlgorithmParameters)) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }

    var algorithmName = normalizedAlgorithmParameters.name;
    var hashAlgorithmName;
    if (algorithmName === 'RSASSA-PKCS1-v1_5') {
      // The hash algorithm when signing with RSASSA-PKCS1-v1_5 is specified at
      // key generation in RsaHashedKeyGenParameters. For more information about
      // RSA key generation parameters, please refer to:
      // https://www.w3.org/TR/WebCryptoAPI/#RsaHashedKeyGenParams-dictionary
      hashAlgorithmName = key.algorithm.hash.name;
    } else if (algorithmName === 'ECDSA') {
      // The hash algorithm when signing with ECDSA is specified in the signing
      // parameters. For more information about ECDSA parameters, please refer
      // to: https://www.w3.org/TR/WebCryptoAPI/#dfn-EcdsaParams
      hashAlgorithmName = normalizedAlgorithmParameters.hash.name;
    }
    // Create an ArrayBuffer that equals the dataView. Note that dataView.buffer
    // might contain more data than dataView.
    var data = dataView.buffer.slice(dataView.byteOffset,
                                     dataView.byteOffset + dataView.byteLength);
    internalAPI.sign(
        subtleCrypto.tokenId, getKeyIdentifier(key),
        normalizedAlgorithmParameters.name, hashAlgorithmName, data,
        function(signature) {
          if (catchInvalidTokenError(reject)) {
            return;
          }
          if (bindingUtil.hasLastError()) {
            bindingUtil.clearLastError();
            reject(CreateOperationError());
            return;
          }
          resolve(signature);
        });
  });
};

SubtleCryptoImpl.prototype.exportKey = function(format, key) {
  return new Promise(function(resolve, reject) {
    if (format === 'pkcs8') {
      // The 'pkcs8' format is intended for 'private' keys, which are always
      // non-extractable in this API. The 'raw' format is intended for 'secret'
      // keys and could also be handled with |InvalidAccessError|, but is
      // actually handled with |NotSupportedError| below, for legacy reasons.
      throw CreateInvalidAccessError();
    } else if (format === 'spki') {
      if (key.type !== 'public') {
        throw CreateInvalidAccessError();
      }
      resolve(getKeyIdentifier(key));
    } else {
      // All other exporting formats are unsupported.
      // TODO(pneubeck): It should be possible to export to format 'jwk'.
      throw CreateNotSupportedError();
    }
  });
};

function SubtleCrypto() {
  privates(SubtleCrypto).constructPrivate(this, arguments);
}
utils.expose(SubtleCrypto, SubtleCryptoImpl, {
  functions: [
    'sign',
    'exportKey',
  ],
});

// Required for sub-classing.
exports.$set('catchInvalidTokenError', catchInvalidTokenError);
exports.$set('SubtleCryptoImpl', SubtleCryptoImpl);
exports.$set('SubtleCrypto', SubtleCrypto);
