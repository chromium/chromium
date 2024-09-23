// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the platformKeys API.

var SubtleCrypto = require('platformKeys.SubtleCrypto').SubtleCrypto;
var cryptoKeyUtil = require('platformKeys.getCryptoKeyUtil');
var getPublicKey = cryptoKeyUtil.getPublicKey;
var getPublicKeyBySpki = cryptoKeyUtil.getPublicKeyBySpki;
var getSymKeyById = cryptoKeyUtil.getSymKeyById;
var internalAPI = getInternalApi('platformKeysInternal');

var keyModule = require('platformKeys.Key');
var Key = keyModule.Key;
var KeyType = keyModule.KeyType;
var KeyUsage = keyModule.KeyUsage;

// TODO(b/288880151): replace the fixed `usages` list below with the actual list
// for the given key, which will be returned by the internal API.
function createPublicKey(keyIdentifier, algorithm) {
  return new Key(
      KeyType.public, keyIdentifier, algorithm, [KeyUsage.verify],
      /*extractable=*/ true);
}

// TODO(b/288880151): replace the fixed `usages` list below with the actual list
// for the given key, which will be returned by the internal API.
function createPrivateKey(keyIdentifier, algorithm) {
  return new Key(
      KeyType.private, keyIdentifier, algorithm, [KeyUsage.sign],
      /*extractable=*/ false);
}

// TODO(b/288880151): replace the fixed `usages` list below with the actual list
// for the given key, which will be returned by the internal API.
function createSymKey(keyIdentifier, algorithm) {
  return new Key(
      KeyType.secret, keyIdentifier, algorithm, /*usages=*/[],
      /*extractable=*/ false);
}

apiBridge.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;
  var subtleCrypto = new SubtleCrypto(/*tokenId=*/ '');

  apiFunctions.setHandleRequest(
      'selectClientCertificates', function(details, callback) {
        internalAPI.selectClientCertificates(details, function(matches) {
          if (chrome.runtime.lastError) {
            callback([]);
            return;
          }
          callback($Array.map(matches, function(match) {
            // internalAPI.selectClientCertificates returns publicExponent as
            // ArrayBuffer, but it should be a Uint8Array.
            if (match.keyAlgorithm.publicExponent) {
              match.keyAlgorithm.publicExponent =
                  new Uint8Array(match.keyAlgorithm.publicExponent);
            }
            return match;
          }));
        });
      });

  apiFunctions.setHandleRequest(
      'subtleCrypto', function() { return subtleCrypto });

  apiFunctions.setHandleRequest('getKeyPair', function(cert, params, callback) {
    getPublicKey(cert, params, function(foundKeySpki, foundKeyAlgorithm) {
      if (chrome.runtime.lastError) {
        callback();
        return;
      }
      callback(
          createPublicKey(foundKeySpki, foundKeyAlgorithm),
          createPrivateKey(foundKeySpki, foundKeyAlgorithm));
    });
  });

  apiFunctions.setHandleRequest(
      'getKeyPairBySpki', function(publicKeySpkiDer, params, callback) {
        getPublicKeyBySpki(
            publicKeySpkiDer, params,
            function(foundKeySpki, foundKeyAlgorithm) {
              if (bindingUtil.hasLastError()) {
                callback();
                return;
              }
              callback(
                  createPublicKey(foundKeySpki, foundKeyAlgorithm),
                  createPrivateKey(foundKeySpki, foundKeyAlgorithm));
            });
      });

  apiFunctions.setHandleRequest('getSymKeyById', function(symKeyId, callback) {
    getSymKeyById(symKeyId, function(foundKeyId, foundKeyAlgorithm) {
      if (bindingUtil.hasLastError()) {
        callback();
        return;
      }
      callback(createSymKey(foundKeyId, foundKeyAlgorithm));
    });
  });
});
