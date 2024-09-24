// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40735021): Refactor enterprise.platformKeys test extension.

'use strict';

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var assertThrows = chrome.test.assertThrows;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;
var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

// True if the C++ side of the test has configured the test to run in a user
// session.
var isUserSessionTest;
// True if the C++ side of the test has enabled a system token for testing.
var systemTokenEnabled;

// openssl req -new -x509 -key privkey.pem \
//   -outform der -out cert.der -days 36500
// xxd -i cert.der
// Based on privateKeyPkcs8User, which is stored in the user's token.
var cert1a = new Uint8Array([
  0x30, 0x82, 0x01, 0xd5, 0x30, 0x82, 0x01, 0x7f, 0xa0, 0x03, 0x02, 0x01, 0x02,
  0x02, 0x09, 0x00, 0xd2, 0xcc, 0x76, 0xeb, 0x19, 0xb9, 0x3a, 0x33, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00,
  0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a,
  0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21, 0x30,
  0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65, 0x72,
  0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50,
  0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
  0x34, 0x31, 0x35, 0x31, 0x34, 0x35, 0x32, 0x30, 0x33, 0x5a, 0x18, 0x0f, 0x32,
  0x31, 0x31, 0x34, 0x30, 0x33, 0x32, 0x32, 0x31, 0x34, 0x35, 0x32, 0x30, 0x33,
  0x5a, 0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
  0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21,
  0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65,
  0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20,
  0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b,
  0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xc7, 0xc1, 0x4d, 0xd5, 0xdc, 0x3a, 0x2e,
  0x1f, 0x42, 0x30, 0x3d, 0x21, 0x1e, 0xa2, 0x1f, 0x60, 0xcb, 0x71, 0x11, 0x53,
  0xb0, 0x75, 0xa0, 0x62, 0xfe, 0x5e, 0x0a, 0xde, 0xb0, 0x0f, 0x48, 0x97, 0x5e,
  0x42, 0xa7, 0x3a, 0xd1, 0xca, 0x4c, 0xe3, 0xdb, 0x5f, 0x31, 0xc2, 0x99, 0x08,
  0x89, 0xcd, 0x6d, 0x20, 0xaa, 0x75, 0xe6, 0x2b, 0x98, 0xd2, 0xf3, 0x7b, 0x4b,
  0xe5, 0x9b, 0xfe, 0xe2, 0x6d, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x50, 0x30,
  0x4e, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xbd,
  0x85, 0x6b, 0xdd, 0x84, 0xd1, 0x54, 0x2e, 0xad, 0xb4, 0x5e, 0xdd, 0x24, 0x7e,
  0x16, 0x9c, 0x84, 0x1e, 0x19, 0xf0, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23,
  0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0xbd, 0x85, 0x6b, 0xdd, 0x84, 0xd1, 0x54,
  0x2e, 0xad, 0xb4, 0x5e, 0xdd, 0x24, 0x7e, 0x16, 0x9c, 0x84, 0x1e, 0x19, 0xf0,
  0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01,
  0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0x37, 0x23, 0x2f, 0x81, 0x24, 0xfc, 0xec,
  0x2d, 0x0b, 0xd1, 0xa0, 0x74, 0xdf, 0x2e, 0x34, 0x9a, 0x92, 0x33, 0xae, 0x75,
  0xd6, 0x60, 0xfc, 0x44, 0x1d, 0x65, 0x8c, 0xb7, 0xd9, 0x60, 0x3b, 0xc7, 0x20,
  0x30, 0xdf, 0x17, 0x07, 0xd1, 0x87, 0xda, 0x2b, 0x7f, 0x84, 0xf3, 0xfc, 0xb0,
  0x31, 0x42, 0x08, 0x17, 0x96, 0xd2, 0x1b, 0xdc, 0x28, 0xae, 0xf8, 0xbd, 0xf9,
  0x4e, 0x78, 0xc3, 0xe8, 0x80
]);

// Based on privateKeyPkcs8User, different from cert1a.
var cert1b = new Uint8Array([
  0x30, 0x82, 0x01, 0xd5, 0x30, 0x82, 0x01, 0x7f, 0xa0, 0x03, 0x02, 0x01, 0x02,
  0x02, 0x09, 0x00, 0xe7, 0x1e, 0x6e, 0xb0, 0x12, 0x87, 0xf5, 0x09, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00,
  0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a,
  0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21, 0x30,
  0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65, 0x72,
  0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50,
  0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
  0x34, 0x31, 0x35, 0x31, 0x35, 0x31, 0x39, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x32,
  0x31, 0x31, 0x34, 0x30, 0x33, 0x32, 0x32, 0x31, 0x35, 0x31, 0x39, 0x30, 0x30,
  0x5a, 0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
  0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21,
  0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65,
  0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20,
  0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b,
  0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xc7, 0xc1, 0x4d, 0xd5, 0xdc, 0x3a, 0x2e,
  0x1f, 0x42, 0x30, 0x3d, 0x21, 0x1e, 0xa2, 0x1f, 0x60, 0xcb, 0x71, 0x11, 0x53,
  0xb0, 0x75, 0xa0, 0x62, 0xfe, 0x5e, 0x0a, 0xde, 0xb0, 0x0f, 0x48, 0x97, 0x5e,
  0x42, 0xa7, 0x3a, 0xd1, 0xca, 0x4c, 0xe3, 0xdb, 0x5f, 0x31, 0xc2, 0x99, 0x08,
  0x89, 0xcd, 0x6d, 0x20, 0xaa, 0x75, 0xe6, 0x2b, 0x98, 0xd2, 0xf3, 0x7b, 0x4b,
  0xe5, 0x9b, 0xfe, 0xe2, 0x6d, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x50, 0x30,
  0x4e, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xbd,
  0x85, 0x6b, 0xdd, 0x84, 0xd1, 0x54, 0x2e, 0xad, 0xb4, 0x5e, 0xdd, 0x24, 0x7e,
  0x16, 0x9c, 0x84, 0x1e, 0x19, 0xf0, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23,
  0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0xbd, 0x85, 0x6b, 0xdd, 0x84, 0xd1, 0x54,
  0x2e, 0xad, 0xb4, 0x5e, 0xdd, 0x24, 0x7e, 0x16, 0x9c, 0x84, 0x1e, 0x19, 0xf0,
  0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01,
  0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0x82, 0x95, 0xa7, 0x08, 0x6c, 0xbd, 0x49,
  0xe6, 0x1e, 0xc1, 0xd9, 0x58, 0x54, 0x11, 0x11, 0x84, 0x77, 0x1e, 0xad, 0xe9,
  0x73, 0x69, 0x1c, 0x5c, 0xaa, 0x26, 0x3e, 0x5f, 0x1d, 0x89, 0x20, 0xc3, 0x90,
  0xa4, 0x67, 0xfa, 0x26, 0x20, 0xd7, 0x1f, 0xae, 0x42, 0x89, 0x30, 0x61, 0x43,
  0x8a, 0x8c, 0xbe, 0xd4, 0x32, 0xf7, 0x96, 0x71, 0x2a, 0xcd, 0xeb, 0x26, 0xf6,
  0xdb, 0x54, 0x95, 0xca, 0x5a
]);

// Based on a private key different than privateKeyPkcs8User or
// privateKeyPkcs8System.
var cert2 = new Uint8Array([
  0x30, 0x82, 0x01, 0xd5, 0x30, 0x82, 0x01, 0x7f, 0xa0, 0x03, 0x02, 0x01, 0x02,
  0x02, 0x09, 0x00, 0x9e, 0x11, 0x7e, 0xff, 0x43, 0x84, 0xd4, 0xe6, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00,
  0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a,
  0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21, 0x30,
  0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65, 0x72,
  0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50,
  0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
  0x34, 0x30, 0x37, 0x31, 0x35, 0x35, 0x30, 0x30, 0x38, 0x5a, 0x18, 0x0f, 0x32,
  0x31, 0x31, 0x34, 0x30, 0x33, 0x31, 0x34, 0x31, 0x35, 0x35, 0x30, 0x30, 0x38,
  0x5a, 0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
  0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21,
  0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65,
  0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20,
  0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b,
  0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xac, 0x6c, 0x72, 0x46, 0xa2, 0xde, 0x88,
  0x30, 0x54, 0x06, 0xad, 0xc7, 0x2d, 0x64, 0x6e, 0xf6, 0x0f, 0x72, 0x3e, 0x92,
  0x31, 0xcc, 0x0b, 0xa0, 0x18, 0x20, 0xb0, 0xdb, 0x86, 0xab, 0x11, 0xc6, 0xa5,
  0x78, 0xea, 0x64, 0xe8, 0xeb, 0xa5, 0xb3, 0x78, 0x5d, 0xbb, 0x10, 0x57, 0xe6,
  0x12, 0x23, 0x89, 0x92, 0x1d, 0xa0, 0xe5, 0x1e, 0xd1, 0xc9, 0x0e, 0x62, 0xcb,
  0xc9, 0xaf, 0xde, 0x4e, 0x83, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x50, 0x30,
  0x4e, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x75,
  0x6c, 0x61, 0xfb, 0xb0, 0x6e, 0x37, 0x32, 0x41, 0x62, 0x3b, 0x55, 0xbd, 0x5f,
  0x6b, 0xe0, 0xdb, 0xb9, 0xc7, 0xec, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23,
  0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x75, 0x6c, 0x61, 0xfb, 0xb0, 0x6e, 0x37,
  0x32, 0x41, 0x62, 0x3b, 0x55, 0xbd, 0x5f, 0x6b, 0xe0, 0xdb, 0xb9, 0xc7, 0xec,
  0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01,
  0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0xa5, 0xe8, 0x9d, 0x3d, 0xc4, 0x1a, 0x6e,
  0xd2, 0x92, 0x42, 0x37, 0xb9, 0x3a, 0xb3, 0x8e, 0x2f, 0x55, 0xb5, 0xf2, 0xe4,
  0x6e, 0x39, 0x0d, 0xa8, 0xba, 0x10, 0x43, 0x57, 0xdd, 0x4e, 0x4e, 0x52, 0xc6,
  0xbe, 0x07, 0xdb, 0x83, 0x05, 0x97, 0x97, 0xc1, 0x7b, 0xd5, 0x5c, 0x50, 0x64,
  0x0f, 0x96, 0xff, 0x3d, 0x83, 0x37, 0x8f, 0x3a, 0x85, 0x08, 0x62, 0x5c, 0xb1,
  0x2f, 0x68, 0xb2, 0x4a, 0x4a
]);

// Based on privateKeyPkcs8System, which is stored in the system token.
var certSystem = new Uint8Array([
  0x30, 0x82, 0x01, 0xd5, 0x30, 0x82, 0x01, 0x7f, 0xa0, 0x03, 0x02, 0x01, 0x02,
  0x02, 0x09, 0x00, 0xf4, 0x3d, 0x9f, 0xd2, 0x1e, 0xa4, 0xf5, 0x82, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00,
  0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a,
  0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21, 0x30,
  0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65, 0x72,
  0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50,
  0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
  0x37, 0x32, 0x38, 0x31, 0x33, 0x31, 0x36, 0x34, 0x35, 0x5a, 0x18, 0x0f, 0x32,
  0x31, 0x31, 0x34, 0x30, 0x37, 0x30, 0x34, 0x31, 0x33, 0x31, 0x36, 0x34, 0x35,
  0x5a, 0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
  0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21,
  0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65,
  0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20,
  0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b,
  0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xe8, 0xb3, 0x04, 0xb1, 0xad, 0xef, 0x6b,
  0xe5, 0xbe, 0xc9, 0x05, 0x75, 0x07, 0x41, 0xf5, 0x70, 0x50, 0xc2, 0xe8, 0xee,
  0xeb, 0x09, 0x9d, 0x49, 0x64, 0x4c, 0x60, 0x61, 0x80, 0xbe, 0xc5, 0x41, 0xf3,
  0x8c, 0x57, 0x90, 0x3a, 0x44, 0x62, 0x6d, 0x51, 0xb8, 0xbb, 0xc6, 0x9a, 0x16,
  0xdf, 0xf9, 0xce, 0xe3, 0xb8, 0x8c, 0x2e, 0xa2, 0x16, 0xc8, 0xed, 0xc7, 0xf8,
  0x4f, 0xbd, 0xd3, 0x6e, 0x63, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x50, 0x30,
  0x4e, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xcd,
  0x97, 0x2d, 0xb2, 0xe2, 0xb8, 0x11, 0xea, 0xcf, 0x0b, 0xca, 0xad, 0x61, 0xf4,
  0x2e, 0x49, 0x3e, 0xa0, 0x7e, 0xa7, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23,
  0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0xcd, 0x97, 0x2d, 0xb2, 0xe2, 0xb8, 0x11,
  0xea, 0xcf, 0x0b, 0xca, 0xad, 0x61, 0xf4, 0x2e, 0x49, 0x3e, 0xa0, 0x7e, 0xa7,
  0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01,
  0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0x8c, 0x05, 0x7e, 0xb1, 0xef, 0x5f, 0x7d,
  0x80, 0x0c, 0x70, 0x9c, 0x99, 0x70, 0x97, 0x5f, 0x83, 0x89, 0xe3, 0x4e, 0x3c,
  0x77, 0xed, 0xf3, 0x66, 0x2d, 0xd6, 0xa9, 0x46, 0x7d, 0xeb, 0x58, 0xbc, 0x50,
  0xa7, 0xe6, 0xd7, 0x7d, 0xfc, 0xdd, 0x18, 0x20, 0x53, 0xfb, 0x11, 0x3d, 0xfc,
  0x2f, 0xf3, 0x30, 0x60, 0x47, 0x2d, 0x8e, 0xd7, 0xbf, 0x0f, 0x0d, 0x47, 0x99,
  0xcc, 0x6d, 0xab, 0xb6, 0xd6
]);

// Random data to be used for signing tests.
const DATA = new Uint8Array([0, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 6]);

/**
 * Runs an array of asynchronous functions [f1, f2, ...] of the form
 *   function(callback) {}
 * by chaining, i.e. f1(f2(...)). Additionally, each callback is wrapped with
 * callbackPass.
 */
function runAsyncSequence(funcs) {
  if (funcs.length == 0)
    return;
  function go(i) {
    var current = funcs[i];
    console.log('#' + (i + 1) + ' of ' + funcs.length);
    if (i == funcs.length - 1) {
      current(callbackPass());
    } else {
      current(callbackPass(go.bind(undefined, i + 1)));
    }
  };
  go(0);
}

// Some array comparison. Note: not lexicographical!
function compareArrays(array1, array2) {
  if (array1.length < array2.length)
    return -1;
  if (array1.length > array2.length)
    return 1;
  for (var i = 0; i < array1.length; i++) {
    if (array1[i] < array2[i])
      return -1;
    if (array1[i] > array2[i])
      return 1;
  }
  return 0;
}

/**
 * @param {ArrayBufferView[]} certs
 * @return {ArrayBufferView[]} |certs| sorted in some order.
 */
function sortCerts(certs) {
  return certs.sort(compareArrays);
}

/**
 * Checks whether the certificates currently stored in |token| match
 * |expectedCerts| by comparing to the result of platformKeys.getCertificates.
 * The order of |expectedCerts| is ignored. Afterwards calls |callback|.
 */
function assertCertsStored(token, expectedCerts, callback) {
  if (!token) {
    if (callback)
      callback();
    return;
  }
  chrome.enterprise.platformKeys.getCertificates(
      token.id, callbackPass(function(actualCerts) {
        assertEq(
            expectedCerts.length, actualCerts.length,
            'Number of stored certs not as expected');
        if (expectedCerts.length == actualCerts.length) {
          actualCerts = actualCerts.map(function(buffer) {
            return new Uint8Array(buffer);
          });
          actualCerts = sortCerts(actualCerts);
          expectedCerts = sortCerts(expectedCerts);
          for (var i = 0; i < expectedCerts.length; i++) {
            assertTrue(
                compareArrays(expectedCerts[i], actualCerts[i]) == 0,
                'Certs at index ' + i + ' differ');
          }
        }
        if (callback)
          callback();
      }));
}

/**
 * Fetches all available tokens using platformKeys.getTokens and calls
 * |callback| with the user and system token if available or with undefined
 * otherwise.
 */
function getTokens(callback) {
  chrome.enterprise.platformKeys.getTokens(function(tokens) {
    var userToken = null;
    var systemToken = null;
    for (var i = 0; i < tokens.length; i++) {
      if (tokens[i].id == 'user')
        userToken = tokens[i];
      else if (tokens[i].id == 'system')
        systemToken = tokens[i];
    }
    callback(userToken, systemToken);
  });
}

function checkApiAvailability() {
  assertTrue(!!chrome.enterprise, 'No enterprise namespace.');
  assertTrue(!!chrome.enterprise.platformKeys, 'No platformKeys namespace.');
  assertTrue(
      !!chrome.enterprise.platformKeys.getTokens, 'No getTokens function.');
  assertTrue(
      !!chrome.enterprise.platformKeys.importCertificate,
      'No importCertificate function.');
  assertTrue(
      !!chrome.enterprise.platformKeys.removeCertificate,
      'No removeCertificate function.');
}

/**
 * Runs preparations before the actual in-user-session tests. Calls |callback|
 * with |userToken| and |systemToken| (if enabled).
 */
function beforeInUserSessionTests(systemTokenEnabled, callback) {
  checkApiAvailability();

  getTokens(function(userToken, systemToken) {
    if (!userToken)
      fail('No user token');
    assertEq('user', userToken.id);

    if (systemTokenEnabled) {
      if (!systemToken)
        fail('No system token');
      assertEq('system', systemToken.id);
    } else {
      assertEq(
          null, systemToken,
          'System token is disabled, but found the token nonetheless.');
    }

    callback(userToken, systemToken);
  });
}

/**
 * Runs preparations before the actual login screen tests. Calls |callback| with
 * the |systemToken|.
 */
function beforeLoginScreenTests(callback) {
  checkApiAvailability();

  getTokens(function(userToken, systemToken) {
    if (userToken) {
      fail('A user token is found on the login screen.');
    }

    if (!systemToken) {
      fail('No system token found.');
    }

    assertEq('system', systemToken.id);
    callback(systemToken);
  });
}

function checkRsaAlgorithmIsCopiedOnRead(key) {
  var algorithm = key.algorithm;
  var originalAlgorithm = {
    name: algorithm.name,
    modulusLength: algorithm.modulusLength,
    publicExponent: algorithm.publicExponent,
    hash: {name: algorithm.hash.name}
  };
  algorithm.hash.name = null;
  algorithm.hash = null;
  algorithm.name = null;
  algorithm.modulusLength = null;
  algorithm.publicExponent = null;
  assertEq(originalAlgorithm, key.algorithm);
}

function checkEcAlgorithmIsCopiedOnRead(key) {
  var algorithm = key.algorithm;
  var originalAlgorithm = {
    name: algorithm.name,
    namedCurve: algorithm.namedCurve,
  };
  algorithm.name = null;
  algorithm.namedCurve = null;
  assertEq(originalAlgorithm, key.algorithm);
}

function checkPropertyIsReadOnly(object, key) {
  var original = object[key];
  try {
    object[key] = {};
    fail('Expected the property to be read-only and an exception to be thrown');
  } catch (error) {
    assertEq(original, object[key]);
  }
}

function checkRsaKeyPairCommonFormat(keyPair) {
  checkPropertyIsReadOnly(keyPair, 'privateKey');
  var privateKey = keyPair.privateKey;
  assertEq('private', privateKey.type);
  assertFalse(privateKey.extractable);
  checkPropertyIsReadOnly(privateKey, 'algorithm');
  checkRsaAlgorithmIsCopiedOnRead(privateKey);

  checkPropertyIsReadOnly(keyPair, 'publicKey');
  var publicKey = keyPair.publicKey;
  assertEq('public', publicKey.type);
  assertTrue(publicKey.extractable);
  checkPropertyIsReadOnly(publicKey, 'algorithm');
  checkRsaAlgorithmIsCopiedOnRead(publicKey);
}

function checkEcKeyPairCommonFormat(keyPair) {
  checkPropertyIsReadOnly(keyPair, 'privateKey');
  var privateKey = keyPair.privateKey;
  assertEq('private', privateKey.type);
  assertFalse(privateKey.extractable);
  checkPropertyIsReadOnly(privateKey, 'algorithm');
  checkEcAlgorithmIsCopiedOnRead(privateKey);

  checkPropertyIsReadOnly(keyPair, 'publicKey');
  var publicKey = keyPair.publicKey;
  assertEq('public', publicKey.type);
  assertTrue(publicKey.extractable);
  checkPropertyIsReadOnly(publicKey, 'algorithm');
  checkEcAlgorithmIsCopiedOnRead(publicKey);
}

// An example of a dictionary that is specified on a RSA |sign| operation.
const RSA_SIGN_ALGORITHM = {
  name: 'RSASSA-PKCS1-v1_5'
};

// Verifies that signing data with RSA |keyPair| works. Error messages will be
// prefixed with |debugMessage|. Returns an array with the first element as the
// generated key pair and the second element as the SubjectPublicKeyInfo.
async function verifyRsaKeySign(
    subtleCrypto, algorithm, keyPair, spki, debugMessage) {

  let signature;
  try {
    signature =
        await subtleCrypto.sign(RSA_SIGN_ALGORITHM, keyPair.privateKey, DATA);
  } catch (error) {
    fail(debugMessage + ': Sign failed: ' + error);
  }

  var importParams = {
    name: algorithm.name,
    // RsaHashedImportParams
    hash: {
      name: algorithm.hash.name,
    }
  };
  assertTrue(!!signature, debugMessage + ': No signature.');
  assertTrue(signature.length != 0, debugMessage + ': Signature is empty.');

  let webCryptoPublicKey;
  try {
    webCryptoPublicKey = await crypto.subtle.importKey(
        'spki', spki, importParams, false, ['verify']);
  } catch (error) {
    fail(debugMessage + ': Import failed: ' + error);
  }

  assertTrue(!!webCryptoPublicKey);
  assertEq(algorithm.modulusLength, webCryptoPublicKey.algorithm.modulusLength);
  assertEq(
      algorithm.publicExponent, webCryptoPublicKey.algorithm.publicExponent);

  let success;
  try {
    success = await crypto.subtle.verify(
        algorithm, webCryptoPublicKey, signature, DATA);
  } catch (error) {
    fail(debugMessage + ': Verification failed: ' + error);
  }

  assertEq(true, success, debugMessage + ': Signature invalid.');
  return [keyPair, spki];
}

// Verifies that signing data with EC |keyPair| works. Error messages will be
// prefixed with |debugMessage|. Returns an array with the first element as the
// generated key pair and the second element as the SubjectPublicKeyInfo.
async function verifyEcKeySign(
    subtleCrypto, params, keyPair, spki, debugMessage) {
  let signature;
  try {
    signature = await subtleCrypto.sign(params.sign, keyPair.privateKey, DATA);
  } catch (error) {
    fail(debugMessage + ': Sign failed: ' + error);
  }

  assertTrue(!!signature, debugMessage + ': No signature.');
  assertTrue(signature.length != 0, debugMessage + ': Signature is empty.');

  let webCryptoPublicKey;
  try {
    webCryptoPublicKey = await crypto.subtle.importKey(
        'spki', spki, params.importKey, false, ['verify']);
  } catch (error) {
    fail(debugMessage + ': Import failed: ' + error);
  }

  assertTrue(!!webCryptoPublicKey);

  let success;
  try {
    success = await crypto.subtle.verify(
        params.verify, webCryptoPublicKey, signature, DATA);
  } catch (error) {
    fail(debugMessage + ': Verification failed: ' + error);
  }

  assertEq(true, success, debugMessage + ': Signature invalid.');
  return [keyPair, spki];
}

// Generates an RSA key with the |algorithm| parameters. Signs random data using
// the new key and verifies the signature using WebCrypto. Returns an array with
// the first element as the generated key pair and the second element as the
// SubjectPublicKeyInfo. Also freezes |algorithm|.
async function generateRsaKeyAndVerify(subtleCrypto, algorithm) {
  // Ensure that this algorithm object is not modified, so that later
  // comparisons really do the right thing.
  Object.freeze(algorithm.hash);
  Object.freeze(algorithm);

  let keyPair;
  try {
    keyPair = await subtleCrypto.generateKey(algorithm, false, ['sign']);
  } catch (error) {
    fail('GenerateKey failed: ' + error);
  }
  assertTrue(!!keyPair, 'No key pair.');

  let publicKeySpki;
  try {
    publicKeySpki = await subtleCrypto.exportKey('spki', keyPair.publicKey);
  } catch (error) {
    fail('Export failed: ' + error);
  }

  // Ensure that the returned key pair has the expected format.
  // Some parameter independent checks:
  checkRsaKeyPairCommonFormat(keyPair);

  // Checks depending on the generateKey arguments:
  var privateKey = keyPair.privateKey;
  assertEq(['sign'], privateKey.usages);
  assertEq(algorithm, privateKey.algorithm);

  var publicKey = keyPair.publicKey;
  assertEq([], publicKey.usages);
  assertEq(algorithm, publicKey.algorithm);

  return verifyRsaKeySign(
      subtleCrypto, algorithm, keyPair, publicKeySpki,
      /*debugMessage=*/ 'First signing attempt');
}

// Generates an EC key with the |generateKey| algorithm in params. Signs random
// data using the new key and verifies the signature using WebCrypto. Returns an
// array with the first element as the generated key pair and the second element
// as the SubjectPublicKeyInfo.
async function generateEcKeyAndVerify(subtleCrypto, params) {
  let keyPair;
  try {
    keyPair =
        await subtleCrypto.generateKey(params.generateKey, false, ['sign']);
  } catch (error) {
    fail('GenerateKey failed: ' + error);
  }
  assertTrue(!!keyPair, 'No key pair.');

  let publicKeySpki;
  try {
    publicKeySpki = await subtleCrypto.exportKey('spki', keyPair.publicKey);
  } catch (error) {
    fail('Export failed: ' + error);
  }

  // Ensure that the returned key pair has the expected format.
  // Some parameter independent checks:
  checkEcKeyPairCommonFormat(keyPair);

  // Checks depending on the generateKey arguments:
  var privateKey = keyPair.privateKey;
  assertEq(['sign'], privateKey.usages);

  var publicKey = keyPair.publicKey;
  assertEq([], publicKey.usages);

  return verifyEcKeySign(
      subtleCrypto, params, keyPair, publicKeySpki,
      /*debugMessage=*/ 'First signing attempt');
}

// Generates an AES key with the |algorithm| parameters.
async function generateAesKey(subtleCrypto, algorithm) {
  // TODO(b/288880151): Update test below, after AES key generation is supported
  // in the internal API.
  try {
    await subtleCrypto.generateKey(algorithm, false, []);
    fail('generateKey should fail because it\'s still unsupported internally');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('The algorithm is not supported', error.message);
  }
}

function testInitiallyNoCerts(token) {
  assertCertsStored(token, []);
}

function testHasSubtleCryptoObjects(token) {
  assertTrue(!!token.subtleCrypto, 'token has no subtleCrypto object');
  assertTrue(
      !!token.softwareBackedSubtleCrypto,
      'token has no softwareBackedSubtleCrypto object');
  succeed();
}

function testHasSubtleCryptoMethods(subtleCrypto) {
  assertTrue(
      !!subtleCrypto.generateKey,
      'subtleCrypto object has no generateKey method');
  assertTrue(!!subtleCrypto.sign, 'subtleCrypto object has no sign method');
  assertTrue(
      !!subtleCrypto.exportKey, 'subtleCrypto object has no exportKey method');
  succeed();
}

// An example of a dictionary that is specified on RSA key generation. The
// parameters are defined by WebCrypto as the RsaHashedKeyGenParameters. For
// more information about RSA key generation parameters, please refer to:
// https://www.w3.org/TR/WebCryptoAPI/#RsaHashedKeyGenParams-dictionary
const RSA_GEN_ALGORITHM = {
  name: 'RSASSA-PKCS1-v1_5',
  modulusLength: 2048,
  // Equivalent to 65537.
  publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
  hash: {
    name: 'SHA-1',
  }
};

// Generates an RSA key pair and signs some data with it. Verifies the signature
// using WebCrypto. Verifies also that a second sign operation fails.
async function testGenerateRsaKeyAndSignAllowedOnce(subtleCrypto) {
  const [keyPair, spki] =
      await generateRsaKeyAndVerify(subtleCrypto, RSA_GEN_ALGORITHM);

  // Try to sign data with the same key a second time, which must fail.
  try {
    await subtleCrypto.sign(RSA_SIGN_ALGORITHM, keyPair.privateKey, DATA);
    fail('Second sign call was expected to fail.');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq(
        'The operation failed for an operation-specific reason', error.message);
    succeed();
  }
}

// Generates an RSA key pair and signs some data with it. Verifies the signature
// using WebCrypto. Verifies also that a second sign operation succeeds.
async function testGenerateRsaKeyAndSignAllowedMultipleTimes(subtleCrypto) {
  const [keyPair, spki] =
      await generateRsaKeyAndVerify(subtleCrypto, RSA_GEN_ALGORITHM);

  // Try to sign data with the same key a second time, which must succeed.
  await verifyRsaKeySign(
      subtleCrypto, RSA_GEN_ALGORITHM, keyPair, spki,
      /*debugMessage=*/ 'Second signing attempt');
  succeed();
}

// Web Crypto ECDSA Operation Params.
// For more information about Web Crypto ECDSA parameters specification,
// please refer to: https://www.w3.org/TR/WebCryptoAPI/#ecdsa
const ECDSA_PARAMS = {
  name: 'ECDSA',
  hash: {name: 'SHA-256'},
};

const EC_KEY_GEN_PARAMS = {
  name: 'ECDSA',
  namedCurve: 'P-256',
};

const EC_KEY_IMPORT_PARAMS = {
  name: 'ECDSA',
  namedCurve: 'P-256',
};

const ALL_ECDSA_PARAMS = {
  sign: ECDSA_PARAMS,
  verify: ECDSA_PARAMS,
  generateKey: EC_KEY_GEN_PARAMS,
  importKey: EC_KEY_IMPORT_PARAMS,
};

// Generates an elliptic curve (EC) key pair and signs some data with it.
// Verifies the signature using WebCrypto. Verifies also that a second sign
// operation fails.
async function testGenerateEcKeyAndSignAllowedOnce(subtleCrypto) {
  const [keyPair, spki] =
      await generateEcKeyAndVerify(subtleCrypto, ALL_ECDSA_PARAMS);

  try {
    // Try to sign data with the same key a second time, which must fail.
    await subtleCrypto.sign(ALL_ECDSA_PARAMS.sign, keyPair.privateKey, DATA);
    fail('Second sign call was expected to fail.');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq(
        'The operation failed for an operation-specific reason', error.message);
    succeed();
  }
}

// Generates an elliptic curve (EC) key pair and signs some data with it.
// Verifies the signature using WebCrypto. Verifies also that a second sign
// operation succeeds.
async function testGenerateEcKeyAndSignAllowedMultipleTimes(subtleCrypto) {
  const [keyPair, spki] =
      await generateEcKeyAndVerify(subtleCrypto, ALL_ECDSA_PARAMS);

  // Try to sign data with the same key a second time, which must succeed.
  await verifyEcKeySign(
      subtleCrypto, ALL_ECDSA_PARAMS, keyPair, spki,
      /*debugMessage=*/ 'Second signing attempt');
  succeed();
}

// An example of a dictionary that is specified on AES key generation. The
// parameters are defined by WebCrypto as the AesKeyGenParams. For more
// information about AES key generation parameters, please refer to:
// https://www.w3.org/TR/WebCryptoAPI/#aes-keygen-params
const AES_GEN_ALGORITHM = {
  name: 'AES-CBC',
  length: 256
};

// Tests the generation of AES keys.
async function testGenerateAesKey(subtleCrypto) {
  await generateAesKey(subtleCrypto, AES_GEN_ALGORITHM);
  succeed();
}

// Generates a key and signs some data with other params. Verifies the signature
// using WebCrypto.
async function testGenerateRsaKeyAndSignOtherParams(subtleCrypto) {
  var algorithm = {
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength: 1024,
    // Equivalent to 65537.
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: {
      name: 'SHA-512',
    }
  };

  await generateRsaKeyAndVerify(subtleCrypto, algorithm);

  succeed();
}

// Call generate RSA key with invalid algorithm param, missing modulusLength.
async function testGenerateRsaKeyParamMissingModulusLength(subtleCrypto) {
  var algorithm = {
    name: 'RSASSA-PKCS1-v1_5',
    // Equivalent to 65537.
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: {
      name: 'SHA-1',
    }
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

// Call generate RSA key with invalid algorithm param, missing publicExponent.
async function testGenerateRsaKeyParamMissingPublicExponent(subtleCrypto) {
  var algorithm = {
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength: 1024,
    hash: {
      name: 'SHA-1',
    }
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

// Call generate RSA key with invalid algorithm param, missing hash.
async function testGenerateRsaKeyParamMissingHash(subtleCrypto) {
  var algorithm = {
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength: 1024,
    // Equivalent to 65537.
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

// Call generate RSA key with invalid algorithm param, unsupported public
// exponent.
async function testGenerateRsaKeyParamUnsupportedPublicExponent(subtleCrypto) {
  var algorithm = {
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength: 2048,
    // Different from 65537.
    publicExponent: new Uint8Array([0x01, 0x01]),
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

async function testGenerateEcKeyParamMissingNamedCurve(subtleCrypto) {
  var algorithm = {
    name: 'ECDSA',
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

async function testGenerateEcKeyParamUnsupportedNamedCurve(subtleCrypto) {
  var algorithm = {
    name: 'ECDSA',
    namedCurve: 'P-384',
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, ['sign']);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('The algorithm is not supported', error.message);
    succeed();
  }
}

// Call generate AES key with invalid algorithm param, missing length.
async function testGenerateAesKeyParamMissingLength(subtleCrypto) {
  var algorithm = {
    name: 'AES-CBC',
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, []);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('A required parameter was missing or out-of-range', error.message);
    succeed();
  }
}

// Call generate AES key with invalid algorithm param, unsupported length.
async function testGenerateAesKeyParamUnsupportedLength(subtleCrypto) {
  var algorithm = {
    name: 'AES-CBC',
    length: 128,
  };

  try {
    await subtleCrypto.generateKey(algorithm, false, []);
    fail('generateKey was expected to fail');
  } catch (error) {
    assertTrue(error instanceof Error);
    assertEq('The algorithm is not supported', error.message);
    succeed();
  }
}

function testImportInvalidCert(token) {
  var invalidCert = new ArrayBuffer(16);
  chrome.enterprise.platformKeys.importCertificate(
      token.id, invalidCert,
      callbackFail('Certificate is not a valid X.509 certificate.'));
}

function testRemoveUnknownCert(token) {
  chrome.enterprise.platformKeys.removeCertificate(
      token.id, cert2.buffer, callbackFail('Certificate could not be found.'));
}

function testRemoveInvalidCert(token) {
  var invalidCert = new ArrayBuffer(16);
  chrome.enterprise.platformKeys.removeCertificate(
      token.id, invalidCert,
      callbackFail('Certificate is not a valid X.509 certificate.'));
}

function bindTestsToObject(tests, object) {
  return tests.map(function(test) {
    var bound = test.bind(undefined, object);
    bound.generatedName = test.name;
    return bound;
  });
}

function getUserSessionTests(userToken, systemToken) {
  let tests = getTestsForToken(userToken, /*signMultipleTimes=*/ false);
  if (systemToken) {
    tests = tests.concat(
        getTestsForToken(systemToken, /*signMultipleTimes=*/ false));
  }
  return tests;
}

function getLoginScreenTests(systemToken) {
  return getTestsForToken(systemToken, /*signMultipleTimes=*/ true);
}

function getTestsForToken(token, signMultipleTimes) {
  const tests = bindTestsToObject(
      [
        testHasSubtleCryptoObjects,
        testInitiallyNoCerts,
        testRemoveUnknownCert,
        testImportInvalidCert,
        testRemoveInvalidCert,
      ],
      token);

  const subtleCryptoTests =
      getTestsForSubtleCrypto(token.subtleCrypto, signMultipleTimes);
  const softwareBackedSubtleCryptoTests = getTestsForSubtleCrypto(
      token.softwareBackedSubtleCrypto, signMultipleTimes);

  return tests.concat(subtleCryptoTests, softwareBackedSubtleCryptoTests);
}

function getTestsForSubtleCrypto(subtleCrypto, signMultipleTimes) {
  const tests = [
    testHasSubtleCryptoMethods,
    testGenerateRsaKeyAndSignOtherParams,
    testGenerateRsaKeyParamMissingModulusLength,
    testGenerateRsaKeyParamMissingPublicExponent,
    testGenerateRsaKeyParamMissingHash,
    testGenerateRsaKeyParamUnsupportedPublicExponent,
    testGenerateEcKeyParamMissingNamedCurve,
    testGenerateEcKeyParamUnsupportedNamedCurve,
    testGenerateAesKey,
    testGenerateAesKeyParamMissingLength,
    testGenerateAesKeyParamUnsupportedLength,
  ];

  const generateAndSignTests = getGenerateAndSignTests(signMultipleTimes);

  return bindTestsToObject(tests.concat(generateAndSignTests), subtleCrypto);
}

function getGenerateAndSignTests(signMultipleTimes) {
  if (signMultipleTimes) {
    return [
      testGenerateRsaKeyAndSignAllowedMultipleTimes,
      testGenerateEcKeyAndSignAllowedMultipleTimes,
    ];
  }
  return [
    testGenerateRsaKeyAndSignAllowedOnce,
    testGenerateEcKeyAndSignAllowedOnce,
  ];
}

function runInUserSessionTests(userToken, systemToken) {
  const testsIndependentOfKeys = getUserSessionTests(userToken, systemToken);

  // These tests are not parameterized and work with the keys loaded by the C++
  // side and potentially remove these keys from the tokens.
  const testsNotParameterized = [
    // Importing a cert should fail, if the private key is stored in another
    // token. This uses the certs that refers to the privateKeyPkcs8User and
    // privateKeyPkcs8System keys, which were imported on C++'s side.
    function importCertWithKeyInOtherToken() {
      if (!systemToken) {
        succeed();
        return;
      }

      function importToSystemWithKeyInUserToken(callback) {
        chrome.enterprise.platformKeys.importCertificate(
            systemToken.id, cert1a.buffer,
            callbackFail('Key not found.', callback));
      }
      function importToUserWithKeyInSystemToken(callback) {
        chrome.enterprise.platformKeys.importCertificate(
            userToken.id, certSystem.buffer,
            callbackFail('Key not found.', callback));
      }

      importToSystemWithKeyInUserToken(
          importToUserWithKeyInSystemToken.bind(null, null));
    },

    // Imports and removes certificates for privateKeyPkcs8User and
    // privateKeyPkcs8System (if the system token is enabled), which were
    // imported on C++'s side. Note: After this test, privateKeyPkcs8User and
    // privateKeyPkcs8System are not stored anymore!
    function importAndRemoveCerts() {
      if (systemToken) {
        runAsyncSequence([
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1a]),

          // Importing the same cert again shouldn't change anything.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1a]),

          // The system token should still be empty.
          assertCertsStored.bind(null, systemToken, []),

          // Importing to the system token should not affect the user token.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, systemToken.id, certSystem.buffer),
          assertCertsStored.bind(null, systemToken, [certSystem]),
          assertCertsStored.bind(null, userToken, [cert1a]),

          // Importing the same cert again to the system token shouldn't
          // change anything.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, systemToken.id, certSystem.buffer),
          assertCertsStored.bind(null, systemToken, [certSystem]),

          // Importing another certificate should succeed.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1b.buffer),
          assertCertsStored.bind(null, userToken, [cert1a, cert1b]),

          // Remove cert1a.
          chrome.enterprise.platformKeys.removeCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1b]),

          // Remove certSystem.
          chrome.enterprise.platformKeys.removeCertificate.bind(
              null, systemToken.id, certSystem.buffer),
          assertCertsStored.bind(null, systemToken, []),
          assertCertsStored.bind(null, userToken, [cert1b]),

          // Remove cert1b.
          chrome.enterprise.platformKeys.removeCertificate.bind(
              null, userToken.id, cert1b.buffer),
          assertCertsStored.bind(null, userToken, [])
        ]);
      } else {
        runAsyncSequence([
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1a]),
          // Importing the same cert again shouldn't change anything.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1a]),
          // Importing another certificate should succeed.
          chrome.enterprise.platformKeys.importCertificate.bind(
              null, userToken.id, cert1b.buffer),
          assertCertsStored.bind(null, userToken, [cert1a, cert1b]),
          chrome.enterprise.platformKeys.removeCertificate.bind(
              null, userToken.id, cert1a.buffer),
          assertCertsStored.bind(null, userToken, [cert1b]),
          chrome.enterprise.platformKeys.removeCertificate.bind(
              null, userToken.id, cert1b.buffer),
          assertCertsStored.bind(null, userToken, [])
        ]);
      }
    },

    function getCertsInvalidToken() {
      chrome.enterprise.platformKeys.getCertificates(
          'invalid token id', callbackFail('The token is not valid.'));
    },

    // Imports a certificate for which no private key was imported/generated
    // before.
    function missingPrivateKeyUserToken() {
      chrome.enterprise.platformKeys.importCertificate(
          userToken.id, cert2.buffer, callbackFail('Key not found.'));
    },

    function missingPrivateKeySystemToken() {
      if (!systemToken) {
        succeed();
        return;
      }
      chrome.enterprise.platformKeys.importCertificate(
          systemToken.id, certSystem.buffer, callbackFail('Key not found.'));
    }
  ];

  chrome.test.runTests(testsIndependentOfKeys.concat(testsNotParameterized));
}

function runLoginScreenTests(systemToken) {
  // The test extension need to be allowlisted for some extension features to be
  // allowed to run on login screen. Currently, there is no way to allowlist the
  // extension for a subset of features. Currently, we allowlist the extension
  // for all features using the command line switch --allowlisted-extension-id.
  // One of these features is key_permissions_in_login_screen, which will allow
  // the extension to sign with the generated corporate keys more than once.
  chrome.test.runTests(getLoginScreenTests(systemToken));
}

chrome.test.getConfig(function(config) {
  const args = JSON.parse(config.customArg);
  // Keys of the args map are set by the C++ side to the JS side of the test.
  // NOTE: the keys must stay in sync with the C++ side of the test.
  isUserSessionTest = args.isUserSessionTest;
  systemTokenEnabled = args.systemTokenEnabled;

  if (isUserSessionTest) {
    beforeInUserSessionTests(systemTokenEnabled, runInUserSessionTests);
    return;
  }

  beforeLoginScreenTests(runLoginScreenTests);
});
