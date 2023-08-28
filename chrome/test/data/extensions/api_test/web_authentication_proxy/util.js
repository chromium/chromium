// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dummy JSON-encoded PublicKeyCredential for completeCreateRequest(). The
// credential ID is base64url('test') = 'dGVzdA'.
const MAKE_CREDENTIAL_RESPONSE_JSON = `{
  "id": "dGVzdA",
  "rawId": "dGVzdA",
  "type": "public-key",
  "authenticatorAttachment": "cross-platform",
  "response": {
    "attestationObject": "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YVjE5FMp0DogaNHK9_e7CulU5rDmJZdF8y9IKfdQ8FAR-cJBAAAAAAAAAAAAAAAAAAAAAAAAAAAAQKnIoE6PUxtEEyfXqdBqSnQ6yPhGtof1L50MYa1JOtmfS5XD0Q7BzH-yYKi1D-BrdMMquwW8DBfzxAtUatWsSFGlAQIDJiABIVggqInVFbKi0k_Qd2WH9kK4hZnhXPjhWlRqTtQxoyros1IiWCCo9UskSZuzG14q_dREih7thij6Kj-YvwSd86USfrV5fA",
    "authenticatorData": "5FMp0DogaNHK9_e7CulU5rDmJZdF8y9IKfdQ8FAR-cJBAAAAAAAAAAAAAAAAAAAAAAAAAAAAQKnIoE6PUxtEEyfXqdBqSnQ6yPhGtof1L50MYa1JOtmfS5XD0Q7BzH-yYKi1D-BrdMMquwW8DBfzxAtUatWsSFGlAQIDJiABIVggqInVFbKi0k_Qd2WH9kK4hZnhXPjhWlRqTtQxoyros1IiWCCo9UskSZuzG14q_dREih7thij6Kj-YvwSd86USfrV5fA",
    "clientDataJSON": "eyJ0eXBlIjoid2ViYXV0aG4uY3JlYXRlIiwiY2hhbGxlbmdlIjoiZEdWemRBIiwib3JpZ2luIjoiaHR0cHM6Ly9leGFtcGxlLmNvbSIsImNyb3NzT3JpZ2luIjpmYWxzZX0",
    "publicKey": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEqInVFbKi0k_Qd2WH9kK4hZnhXPjhWlRqTtQxoyros1Ko9UskSZuzG14q_dREih7thij6Kj-YvwSd86USfrV5fA",
    "publicKeyAlgorithm": -7,
    "transports": ["usb"]
  },
  "clientExtensionResults": {}
}`;

// A dummy JSON-encoded PublicKeyCredential for completeGetRequest(). The
// credential ID is base64url('test') = 'dGVzdA'.
const GET_ASSERTION_RESPONSE_JSON = `{
  "id": "dGVzdA",
  "rawId": "dGVzdA",
  "type": "public-key",
  "authenticatorAttachment": "cross-platform",
  "response": {
    "authenticatorData": "YoNLjwSfqzThzqXUg6At1bvcOxxscAyaoCRefuCi6I0BAAAAAA",
    "clientDataJSON": "eyJ0eXBlIjoid2ViYXV0aG4uY3JlYXRlIiwiY2hhbGxlbmdlIjoiZEdWemRBIiwib3JpZ2luIjoiaHR0cHM6Ly9leGFtcGxlLmNvbSIsImNyb3NzT3JpZ2luIjpmYWxzZX0",
    "signature": "RTAhAoIAbL78xmC6MWDpx8-SN1FlNUXo2VcqwxDeNukhh5diAtpUINntpYqNyzR4JaEmhEBdgnHBv82bW-2LZj1l6CgzKABz",
    "userHandle": "dXNlcklk"
  },
  "clientExtensionResults": {}
}`;

const TEST_ERROR_MESSAGE = 'test error message';

// Completes the request with the given request ID using the fake response in
// `MAKE_CREDENTIAL_RESPONSE_JSON`.
export function completeCreateRequest(requestId, optErrorName) {
  let response = {
    requestId: requestId,
  };
  if (optErrorName) {
    response.error = {name: optErrorName, message: TEST_ERROR_MESSAGE};
  } else {
    response.responseJson = MAKE_CREDENTIAL_RESPONSE_JSON;
  }
  return chrome.webAuthenticationProxy.completeCreateRequest(response);
}

// Completes the request with the given request ID using the fake response in
// `GET_ASSERTION_RESPONSE_JSON`.
export function completeGetRequest(requestId, optErrorName) {
  let response = {
    requestId: requestId,
  };
  if (optErrorName) {
    response.error = {name: optErrorName, message: TEST_ERROR_MESSAGE};
  } else {
    response.responseJson = GET_ASSERTION_RESPONSE_JSON;
  }
  return chrome.webAuthenticationProxy.completeGetRequest(response);
}
