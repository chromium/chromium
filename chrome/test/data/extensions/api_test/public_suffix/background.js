// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function knownSuffix() {
    chrome.test.assertTrue(chrome.publicSuffix.isKnownSuffix('com'));
    chrome.test.assertTrue(chrome.publicSuffix.isKnownSuffix('COM'));
    chrome.test.assertTrue(chrome.publicSuffix.isKnownSuffix('co.uk'));
    chrome.test.assertFalse(chrome.publicSuffix.isKnownSuffix('foo.co.uk'));

    chrome.test.assertTrue(chrome.publicSuffix.isKnownSuffix('io'));
    chrome.test.assertFalse(chrome.publicSuffix.isKnownSuffix('foo.io'));
    chrome.test.assertTrue(chrome.publicSuffix.isKnownSuffix('github.io'));
    chrome.test.assertFalse(chrome.publicSuffix.isKnownSuffix('foo.github.io'));

    chrome.test.assertEq('com', chrome.publicSuffix.getKnownSuffix('com'));
    chrome.test.assertEq('com', chrome.publicSuffix.getKnownSuffix('COM'));
    chrome.test.assertEq('com', chrome.publicSuffix.getKnownSuffix('foo.com'));
    chrome.test.assertEq(
        'co.uk', chrome.publicSuffix.getKnownSuffix('example.co.uk'));
    chrome.test.assertEq(
        'co.uk', chrome.publicSuffix.getKnownSuffix('www.example.co.uk'));

    chrome.test.assertEq('io', chrome.publicSuffix.getKnownSuffix('io'));
    chrome.test.assertEq('io', chrome.publicSuffix.getKnownSuffix('foo.io'));
    chrome.test.assertEq(
        'github.io', chrome.publicSuffix.getKnownSuffix('github.io'));
    chrome.test.assertEq(
        'github.io', chrome.publicSuffix.getKnownSuffix('foo.github.io'));

    chrome.test.assertEq(false, chrome.publicSuffix.isKnownSuffix('localhost'));
    chrome.test.assertEq(null, chrome.publicSuffix.getKnownSuffix('localhost'));

    chrome.test.succeed();
  },

  function getDomainDefaults() {
    chrome.test.assertEq(
        'domain.com', chrome.publicSuffix.getDomain('sub.domain.com'));
    chrome.test.assertEq(
        'example.co.uk',
        chrome.publicSuffix.getDomain('www.mail.example.co.uk'));
    chrome.test.assertEq(null, chrome.publicSuffix.getDomain('com'));
    chrome.test.assertEq(null, chrome.publicSuffix.getDomain('co.uk'));

    chrome.test.assertEq('foo.io', chrome.publicSuffix.getDomain('foo.io'));
    chrome.test.assertEq(
        'foo.github.io', chrome.publicSuffix.getDomain('foo.github.io'));
    chrome.test.assertEq(null, chrome.publicSuffix.getDomain('github.io'));

    chrome.test.assertEq(null, chrome.publicSuffix.getDomain('localhost'));
    chrome.test.assertEq(null, chrome.publicSuffix.getDomain('192.168.2.1'));
    chrome.test.succeed();
  },

  function options() {
    chrome.test.assertEq(
        'sub.localhost',
        chrome.publicSuffix.getDomain(
            'foo.sub.localhost', {allowUnknownSuffix: true}));
    chrome.test.assertEq(
        'localhost',
        chrome.publicSuffix.getDomain('localhost', {allowUnknownSuffix: true}));
    chrome.test.assertEq(
        'co.uk',
        chrome.publicSuffix.getDomain('co.uk', {allowPlainSuffix: true}));
    chrome.test.assertEq(
        'github.io',
        chrome.publicSuffix.getDomain('github.io', {allowPlainSuffix: true}));
    chrome.test.assertEq(
        '192.168.2.1',
        chrome.publicSuffix.getDomain('192.168.2.1', {allowIPAddress: true}));
    chrome.test.assertEq('[::1]', chrome.publicSuffix.getDomain('[::1]', {
      allowIPAddress: true,
    }));
    chrome.test.assertEq(
        'example.com', chrome.publicSuffix.getDomain('www.example.com', {
          encoding: 'display',
        }));
    // Unicode confusable, display as punycode.
    chrome.test.assertEq(
        'xn--bs-red.com',
        chrome.publicSuffix.getDomain('xn--bs-red.com', {encoding: 'display'}));
    chrome.test.succeed();
  },

  function invalidHostname() {
    chrome.test.assertThrows(
        () => chrome.publicSuffix.getDomain('https://example.com'),
        /Invalid hostname/);
    chrome.test.assertThrows(
        () => chrome.publicSuffix.getKnownSuffix('https://example.com'),
        /Invalid hostname/);
    chrome.test.assertThrows(
        () => chrome.publicSuffix.isKnownSuffix('https://example.com'),
        /Invalid hostname/);
    chrome.test.succeed();
  },
]);
