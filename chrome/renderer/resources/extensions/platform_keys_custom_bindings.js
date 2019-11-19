// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the platformKeys API.

var SubtleCrypto = require('platformKeys.SubtleCrypto').SubtleCrypto;
var getPublicKey = require('platformKeys.getPublicKey').getPublicKey;
var internalAPI = require('platformKeys.internalAPI');

var keyModule = require('platformKeys.Key');
var Key = keyModule.Key;
var KeyType = keyModule.KeyType;
var KeyUsage = keyModule.KeyUsage;

function createPublicKey(publicKeySpki, algorithm) {
  return new Key(KeyType.public, publicKeySpki, algorithm, [KeyUsage.verify],
                 true /* extractable */);
}

function createPrivateKey(publicKeySpki, algorithm) {
  return new Key(KeyType.private, publicKeySpki, algorithm, [KeyUsage.sign],
                 false /* not extractable */);
}

apiBridge.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;
  var subtleCrypto = new SubtleCrypto('' /* tokenId */);

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

  apiFunctions.setHandleRequest(
      'getKeyPair', function(cert, params, callback) {
        getPublicKey(cert, params, function(publicKey, algorithm) {
          if (chrome.runtime.lastError) {
            callback();
            return;
          }
          callback(createPublicKey(publicKey, algorithm),
                   createPrivateKey(publicKey, algorithm));
        });
      });
});
