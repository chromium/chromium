// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the enterprise.platformKeys API.

// The platformKeys API consists of two major parts:
//   - the certificate management.
//   - the key generation and crypto operations and
// The former is implemented without custom binding as static functions.
// The latter is exposed by implementing WebCrypto's SubtleCrypto interface.
// The internal API provides the key and crypto operations through static
// functions expecting token IDs and this custom binding adds the SubtleCrypto
// wrapper.
// The Token object holds the token id and the SubtleCrypto member.

var Token = require('enterprise.platformKeys.Token').Token;
var internalAPI = require('enterprise.platformKeys.internalAPI');

apiBridge.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  var ret = apiFunctions.setHandleRequest('getTokens', function(callback) {
    internalAPI.getTokens(function(tokenIds) {
      callback($Array.map(tokenIds,
                          function(tokenId) { return new Token(tokenId); }));
    });
  });
});
