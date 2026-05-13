// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary Hash {
  required DOMString name;
};

// For more information about the RSA key generation parameters, please refer
// to: https://www.w3.org/TR/WebCryptoAPI/#RsaHashedKeyGenParams-dictionary
// For more information about the ECDSA key generation parameters, please refer
// to: https://www.w3.org/TR/WebCryptoAPI/#dfn-EcdsaParams
// Note: |hash| is not used by generateKey() but is added to completely mirror
// WebCrypto in using RsaHashedKeyAlgorithm to be able to wrap all parameters in
// one structure.
dictionary Algorithm {
  // Provided for all algorithms.
  required DOMString name;

  // Provided in case the algorithm is RSASSA-PKCS1-v1_5.
  long modulusLength;
  ArrayBuffer publicExponent;
  Hash hash;

  // Provided in case the algorithm is ECDSA.
  DOMString namedCurve;
};

// Internal API for platform keys and certificate management.
[platforms = ("chromeos"),
 implemented_in = "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"]
interface PlatformKeysInternal {
  // Internal version of enterprise.platformKeys.getTokens. Returns a list of
  // token IDs instead of token objects.
  // |Returns|: Invoked by <code>getTokens</code>.
  // |PromiseValue|: tokenIds: The list of IDs of the available Tokens.
  static Promise<sequence<DOMString>> getTokens();

  // Internal version of SubtleCrypto.generateKey, currently supporting only
  // RSASSA-PKCS1-v1_5 and ECDSA.
  // |tokenId|: The id of a Token returned by |getTokens|.
  // |algorithm|: The algorithm parameters as specified by WebCrypto.
  // |softwareBacked|: Whether the key operations should be executed in
  // software.
  // |Returns|: Called back with the Subject Public Key Info of the generated
  // key.
  // |PromiseValue|: publicKey: The Subject Public Key Info (see X.509) of the
  // generated key in DER encoding.
  static Promise<ArrayBuffer> generateKey(
      DOMString tokenId,
      Algorithm algorithm,
      boolean softwareBacked);
};

partial interface Enterprise {
  static attribute PlatformKeysInternal platformKeysInternal;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
