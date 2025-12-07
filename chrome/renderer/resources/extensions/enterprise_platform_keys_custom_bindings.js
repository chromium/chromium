// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the enterprise.platformKeys API.

// The enterprise.platformKeys API consists of two major parts:
//   - the certificate management and
//   - the key generation and crypto operations.
// The former is implemented without custom binding as static functions.
// The latter is exposed by implementing WebCrypto's SubtleCrypto interface.
// The internal API provides the key and crypto operations through static
// functions expecting token IDs and this custom binding adds the SubtleCrypto
// wrapper.
// The Token object holds the token id and the SubtleCrypto member.

const Token = require('enterprise.platformKeys.Token').Token;
const internalAPI = getInternalApi('enterprise.platformKeysInternal');

apiBridge.registerCustomHook(function(api) {
  const apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('getTokens', function(callback) {
    internalAPI.getTokens(function(tokenIds) {
      callback($Array.map(tokenIds, function(tokenId) {
        return new Token(tokenId);
      }));
    });
  });
});
