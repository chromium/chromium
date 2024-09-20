// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_DOMAIN = 'cookies.com';
var TEST_PATH = '/auth';
var TEST_HOST = 'www.chrome_extensions.' + TEST_DOMAIN;
var TEST_URL = 'http://' + TEST_HOST + '/foobar.html?arg=toolbar&param=true';
var TEST_URL_HTTPS =
    'https://' + TEST_HOST + '/foobar.html?arg=toolbar&param=true';
var TEST_URL2 = 'http://chromium.' + TEST_DOMAIN + '/index.html';
var TEST_URL3 = 'https://' + TEST_HOST + '/content.html';
var TEST_URL4 = 'https://' + TEST_HOST + TEST_PATH + '/content.html';
var TEST_URL5 = 'http://' + TEST_HOST + TEST_PATH + '/content.html';
var TEST_OPAQUE_URL = 'file://' + TEST_DOMAIN;
var TEST_EXPIRATION_DATE = 12345678900;
var TEST_ODD_DOMAIN_HOST_ONLY = 'strange stuff!!.com';
var TEST_ODD_DOMAIN = '.' + TEST_ODD_DOMAIN_HOST_ONLY;
var TEST_ODD_PATH = '/hello = world';
var TEST_ODD_URL =
    'http://' + TEST_ODD_DOMAIN_HOST_ONLY + TEST_ODD_PATH + '/index.html';
var TEST_UNPERMITTED_URL = 'http://illegal.' + TEST_DOMAIN + '/';
var TEST_TOP_LEVEL_SITE = 'https://' + TEST_DOMAIN;
var TEST_CROSS_SITE_TOP_LEVEL_SITE = 'https://example.com';

var TEST_BASIC_COOKIE = {
  url: TEST_URL,
  name: 'api_test_basic_cookie',
  value: 'helloworld'
};
var TEST_DOMAIN_COOKIE = {
  url: TEST_URL,
  name: 'TEST_domain',
  value: '32849395FFDSAA**##@@@',
  domain: TEST_DOMAIN,
  expirationDate: TEST_EXPIRATION_DATE
};
var TEST_SECURE_COOKIE = {
  url: TEST_URL4,
  name: 'SECRETCOOKIE',
  value: 'foobar_password',
  secure: true,
  httpOnly: true
};
var TEST_BASIC_EXPIRED_COOKIE = {
  url: TEST_BASIC_COOKIE.url,
  name: TEST_BASIC_COOKIE.name,
  value: TEST_BASIC_COOKIE.value,
  expirationDate: 0
};

var TEST_PARTITIONED_COOKIE = {
  url: TEST_URL4,
  name: 'PARTITIONED_COOKIE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: {topLevelSite: TEST_TOP_LEVEL_SITE, hasCrossSiteAncestor: true}
};
var TEST_UNPARTITIONED_COOKIE = {
  url: TEST_URL4,
  name: 'UNPARTITIONED_COOKIE',
  value: 'unpartitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
};
var TEST_FIRST_PARTY_PARTITIONED_COOKIE = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'FIRST_PARTY_PARTITIONED_COOKIE',
  value: 'TEST_FIRST_PARTY_PARTITIONED_COOKIE_VAL',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: {topLevelSite: TEST_TOP_LEVEL_SITE, hasCrossSiteAncestor: false}
};
var TEST_PARTITIONED_COOKIE_SAME_KEY = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'PARTITIONED_COOKIE_SAME_KEY',
  value: 'partitioned_cookie_same_key',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
};
var TEST_PARTITIONED_COOKIE_DIFFERENT_KEY = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'PARTITIONED_COOKIE_DIFFERENT_KEY',
  value: 'partitioned_cookie_different_key',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: {topLevelSite: TEST_URL5, hasCrossSiteAncestor: true}
};
var TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: TEST_BASIC_COOKIE.name,
  value: 'TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
};
var TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE',
  value: 'TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey:
      {topLevelSite: 'INVALID_PARTITION_KEY', hasCrossSiteAncestor: true}
};
var TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'INVALID_HAS_CROSS_SITE_ANCESTOR',
  value: 'INVALID_HAS_CROSS_SITE_ANCESTOR',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  path: '/',
  partitionKey: {
    topLevelSite: TEST_CROSS_SITE_TOP_LEVEL_SITE,
    hasCrossSiteAncestor: false
  }
};
var TEST_PARTITIONED_COOKIE_SECURE_FALSE = {
  url: TEST_URL,
  name: 'SECURE_FALSE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: false,
  httpOnly: true,
  path: '/',
  partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
};
var TEST_PARTITIONED_COOKIE_OPAQUE_TOP_LEVEL_SITE = {
  url: TEST_PARTITIONED_COOKIE.url,
  name: 'OPAQUE_TOP_LEVEL_SITE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: false,
  httpOnly: true,
  path: '/',
  partitionKey: {topLevelSite: TEST_OPAQUE_URL, hasCrossSiteAncestor: true}
};

function expectValidCookie(cookie) {
  chrome.test.assertNe(null, cookie, 'Expected cookie not set.');
}

function expectNullCookie(cookie) {
  chrome.test.assertEq(null, cookie);
}

function expectUndefinedCookie(cookie) {
  chrome.test.assertEq(undefined, cookie);
}

function removeTestCookies() {
  chrome.cookies.remove(
      {url: TEST_URL, name: TEST_BASIC_COOKIE.name});
  chrome.cookies.remove(
      {url: TEST_URL, name: TEST_DOMAIN_COOKIE.name});
  chrome.cookies.remove(
      {url: TEST_URL4, name: TEST_SECURE_COOKIE.name});
  chrome.cookies.remove({url: TEST_URL, name: 'abcd'});
  chrome.cookies.remove({url: TEST_URL, name: 'AA'});
  chrome.cookies.remove({url: TEST_URL_HTTPS, name: 'A'});
  chrome.cookies.remove({url: TEST_URL, name: 'AI'});
  chrome.cookies.remove({url: TEST_URL, name: 'B'});
  chrome.cookies.remove({url: TEST_URL, name: 'C'});
  chrome.cookies.remove({url: TEST_URL, name: 'D'});
  chrome.cookies.remove({url: TEST_ODD_URL, name: 'abcd'});
  chrome.cookies.remove({
    url: TEST_URL4,
    name: TEST_PARTITIONED_COOKIE.name,
    partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
  });
  chrome.cookies.remove({
    url: TEST_URL4,
    name: TEST_PARTITIONED_COOKIE_SAME_KEY.name,
    partitionKey: TEST_PARTITIONED_COOKIE_SAME_KEY.partitionKey
  });
  chrome.cookies.remove({
    url: TEST_URL4,
    name: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.name,
    partitionKey: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.partitionKey
  });
  chrome.cookies.remove({
    url: TEST_URL4,
    name: TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME.name,
    partitionKey: TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME.partitionKey
  });
  chrome.cookies.remove({
    url: TEST_URL4,
    name: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.name,
    partitionKey: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.partitionKey
  });
  chrome.cookies.remove({
    url: TEST_FIRST_PARTY_PARTITIONED_COOKIE.url,
    name: TEST_FIRST_PARTY_PARTITIONED_COOKIE.name,
    partitionKey: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
  });
}
var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

chrome.test.runTests([
  function invalidScheme() {
    // Invalid schemes don't work with the cookie API.
    var ourUrl = chrome.runtime.getURL('background.js');
    chrome.cookies.get(
        {url: ourUrl, name: 'a'},
        fail('No host permissions for cookies at url: "' + ourUrl + '".'));
  },
  function getBasicCookie() {
    removeTestCookies();
    chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
      // Domain doesn't match.
      chrome.cookies.get(
          {url: TEST_URL2, name: TEST_BASIC_COOKIE.name},
          pass(expectNullCookie));
      // URL invalid.
      chrome.cookies.get(
          {url: 'invalid url', name: TEST_BASIC_COOKIE.name},
          fail('Invalid url: "invalid url".'));
      // URL lacking permissions.
      chrome.cookies.get(
          {url: TEST_UNPERMITTED_URL, name: TEST_BASIC_COOKIE.name},
          fail('No host permissions for cookies at url: "' +
               TEST_UNPERMITTED_URL + '".'));
      // Store ID invalid.
      chrome.cookies.get({
        url: TEST_BASIC_COOKIE.url,
        name: TEST_BASIC_COOKIE.name,
        storeId: 'invalid'
      }, fail('Invalid cookie store id: "invalid".'));
      chrome.cookies.get(
          {url: TEST_BASIC_COOKIE.url, name: TEST_BASIC_COOKIE.name},
          pass(function(cookie) {
            expectValidCookie(cookie);
            chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookie.name);
            chrome.test.assertEq(TEST_BASIC_COOKIE.value, cookie.value);
            chrome.test.assertEq(TEST_HOST, cookie.domain);
            chrome.test.assertEq(true, cookie.hostOnly);
            chrome.test.assertEq('/', cookie.path);
            chrome.test.assertEq(false, cookie.secure);
            chrome.test.assertEq(false, cookie.httpOnly);
            chrome.test.assertEq('unspecified', cookie.sameSite);
            chrome.test.assertEq(true, cookie.session);
            chrome.test.assertTrue(typeof cookie.expirationDate === 'undefined',
                'Session cookie should not have expirationDate property.');
            chrome.test.assertTrue(typeof cookie.storeId !== 'undefined',
                                   'Cookie store ID not provided.');
          }));
    }));
  },
  function getDomainCookie() {
    removeTestCookies();
    chrome.cookies.set(TEST_DOMAIN_COOKIE, pass(function () {
      chrome.cookies.get(
          {url: TEST_URL2, name: TEST_DOMAIN_COOKIE.name},
          pass(function(cookie) {
            expectValidCookie(cookie);
            chrome.test.assertEq(TEST_DOMAIN_COOKIE.name, cookie.name);
            chrome.test.assertEq(TEST_DOMAIN_COOKIE.value, cookie.value);
            chrome.test.assertEq('.' + TEST_DOMAIN, cookie.domain);
            chrome.test.assertEq(false, cookie.hostOnly);
            chrome.test.assertEq('/', cookie.path);
            chrome.test.assertEq(false, cookie.secure);
            chrome.test.assertEq(false, cookie.httpOnly);
            chrome.test.assertEq('unspecified', cookie.sameSite);
            chrome.test.assertEq(false, cookie.session);
            // Expiration is clamped to 400 days, so we test if it's within a
            // minute of 400 days from now.
            const dateDiffInSec = cookie.expirationDate -
                                  Math.round(Date.now()/1000);
            const fourHundredDayInSec = 400 * 24 * 60 * 60;
            chrome.test.assertTrue(
              Math.abs(dateDiffInSec - fourHundredDayInSec) <  60);
          }));
    }));
  },
  function getSecureCookie() {
    removeTestCookies();
    chrome.cookies.set(TEST_SECURE_COOKIE, pass(function () {
      // URL doesn't work because scheme isn't secure.
      chrome.cookies.get(
          {url: TEST_URL5, name: TEST_SECURE_COOKIE.name},
          pass(expectNullCookie));
      // Path doesn't match.
      chrome.cookies.get(
          {url: TEST_URL3, name: TEST_SECURE_COOKIE.name},
          pass(expectNullCookie));
      chrome.cookies.get(
          {url: TEST_URL4, name: TEST_SECURE_COOKIE.name},
          pass(function(cookie) {
            expectValidCookie(cookie);
            chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookie.name);
            chrome.test.assertEq(TEST_SECURE_COOKIE.value, cookie.value);
            chrome.test.assertEq(TEST_HOST, cookie.domain);
            chrome.test.assertEq(true, cookie.hostOnly);
            chrome.test.assertEq(TEST_PATH, cookie.path);
            chrome.test.assertEq(true, cookie.secure);
            chrome.test.assertEq(true, cookie.httpOnly);
            chrome.test.assertEq('unspecified', cookie.sameSite);
            chrome.test.assertEq(true, cookie.session);
          }));
    }));
  },
  function getFirstPartyPartitionedCookie() {
    removeTestCookies();
    chrome.cookies.set(
        TEST_FIRST_PARTY_PARTITIONED_COOKIE, pass(function() {
          // Confirm that cookie can be retrieved with correct values in get
          chrome.cookies.get(
              {
                url: TEST_FIRST_PARTY_PARTITIONED_COOKIE.url,
                name: TEST_FIRST_PARTY_PARTITIONED_COOKIE.name,
                partitionKey: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor,
                    cookie.partitionKey.hasCrossSiteAncestor);
              }));
          // Confirm that cookie can't be retrieved with incorrect value for
          // hasCrossSiteAncestor
          chrome.cookies.get(
              {
                url: TEST_FIRST_PARTY_PARTITIONED_COOKIE.url,
                name: TEST_FIRST_PARTY_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite,
                  hasCrossSiteAncestor: true
                }
              },
              pass(function(cookie) {
                expectNullCookie(cookie);
              }));

          // Confirm that if no hasCrossSiteAncestor is passed first party value
          // can be calculated correctly
          chrome.cookies.get(
              {
                url: TEST_FIRST_PARTY_PARTITIONED_COOKIE.url,
                name: TEST_FIRST_PARTY_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite
                }
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor,
                    cookie.partitionKey.hasCrossSiteAncestor);
              }));
        }))
  },
  function exerciseApisWithPartitionedAndUnpartitionedCookiesPresent() {
    removeTestCookies();
    // These tests exercise the get, getAll and remove methods when partitioned
    // and unpartitioned cookies are present.

    let unpartitioned = structuredClone(TEST_UNPARTITIONED_COOKIE);
    let partitioned = structuredClone(TEST_PARTITIONED_COOKIE);
    // Make names the same so only difference is value and partitionKey.
    unpartitioned.name = 'Cookie';
    partitioned.name = 'Cookie';

    // To ensure the removal of these cookies, a call is made at the end of
    // the test to remove them.
    chrome.cookies.set(unpartitioned);
    chrome.cookies.set(partitioned);

    // GetAll(): passing empty details only gets unpartitioned cookies.
    chrome.cookies.getAll({}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                            chrome.test.assertEq(
                                cookies[0].value, unpartitioned.value);
                          }));

    // GetAll(): passing empty partitionKey gets both unpartitioned and
    // partitioned cookies.
    chrome.cookies.getAll(
        {partitionKey: {}}, pass(function(cookies) {
          chrome.test.assertEq(2, cookies.length);
          chrome.test.assertEq(cookies[0].value, unpartitioned.value);
          chrome.test.assertEq(cookies[1].value, TEST_PARTITIONED_COOKIE.value);
        }));

    // GetAll(): passing a partitionKey with an empty topLevelSite get
    // unpartitioned cookies.
    chrome.cookies.getAll(
        {partitionKey: {topLevelSite: ''}}, pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(cookies[0].value, unpartitioned.value);
        }));

    // GetAll(): passing a partitionKey with an empty topLevelSite gets
    // and a hasCrossSiteAncestor value of true results in an error because an
    // empty topLevelSite indicates an unpartitioned cookie which has no
    // cross-site ancestor.
    chrome.cookies.getAll(
        {partitionKey: {topLevelSite: '', hasCrossSiteAncestor: true}},
        fail(
            'partitionKey with empty topLevelSite unexpectedly has a ' +
            'cross-site ancestor value of true.'));
    // GetAll(): passing in a partitionKey that defaults to unpartitioned, only
    // returns unpartitioned cookies.
    chrome.cookies.getAll(
        {partitionKey: {topLevelSite: '', hasCrossSiteAncestor: false}},
        pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(cookies[0].value, unpartitioned.value);
        }));

    // GetAll(): passing valid partitionKey only gets partitioned cookies.
    chrome.cookies.getAll(
        {partitionKey: TEST_PARTITIONED_COOKIE.partitionKey},
        pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(cookies[0].value, TEST_PARTITIONED_COOKIE.value);
        }));

    // Get(): empty partition key will get an unpartitioned cookie.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
          partitionKey: {},
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(cookie.value, unpartitioned.value);
          chrome.test.assertEq(cookie.partitionKey, null);
        }));

    // Get(): partition key with empty topLevelSite gets unpartitioned cookies.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
          partitionKey: {topLevelSite: ''},
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(cookie.value, unpartitioned.value);
        }));

    // Get(): passing a partitionKey with an empty topLevelSite gets
    // unpartitioned cookies regardless of what is the value of
    // hasCrossSiteAncestor.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
          partitionKey: {topLevelSite: '', hasCrossSiteAncestor: false},
        },
        pass(function(cookie) {
          chrome.test.assertEq(cookie.value, unpartitioned.value);
        }));
    // Get(): passing a partitionKey with an empty topLevelSite gets
    // and a hasCrossSiteAncestor value of true results in an error because an
    // empty topLevelSite indicates an unpartitioned cookie which has no
    // cross-site ancestor.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
          partitionKey: {topLevelSite: '', hasCrossSiteAncestor: true},
        },
        fail(
            'partitionKey with empty topLevelSite unexpectedly has a ' +
            'cross-site ancestor value of true.'));
    // Get(): partitionKey does not get unpartitioned cookies.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
          partitionKey: TEST_PARTITIONED_COOKIE.partitionKey,
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(cookie.value, TEST_PARTITIONED_COOKIE.value);
        }));

    // Get(): no partitionKey does not get partitioned cookies.
    chrome.cookies.get(
        {
          url: unpartitioned.url,
          name: unpartitioned.name,
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(cookie.value, unpartitioned.value);
        }));
    // remove(): empty partitionKey results only unpartitioned cookies being
    // deleted.
    chrome.cookies.remove(
        {name: unpartitioned.name, url: unpartitioned.url, partitionKey: {}},
        pass(async function() {
          chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                                  chrome.test.assertEq(1, cookies.length);
                                  chrome.test.assertEq(
                                      cookies[0].value, partitioned.value);
                                  chrome.cookies.remove({
                                    name: partitioned.name,
                                    url: partitioned.url,
                                    partitionKey: partitioned.partitionKey
                                  });
                                }))
        }));
  },
  function getAllHasCrossSiteAncestor() {
    removeTestCookies();
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    chrome.cookies.set(
        TEST_FIRST_PARTY_PARTITIONED_COOKIE, pass(function() {
          // Confirm expected cookie is present
          chrome.cookies.getAll(
              {partitionKey: {}}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    cookies[0].partitionKey.hasCrossSiteAncestor,
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookies[0].name);
              }));

          // Confirm cookie will be retrieved when no hasCrossSiteAncestorValue
          // provided but topLevelSite is.
          chrome.cookies.getAll(
              {
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    cookies[0].partitionKey.hasCrossSiteAncestor,
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookies[0].name);
              }));

          // check that an undefined hasCrossSiteAncestor gets cookie.
          chrome.cookies.getAll(
              {
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite,
                  hasCrossSiteAncestor: undefined
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    cookies[0].partitionKey.hasCrossSiteAncestor,
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookies[0].name);
              }));

          //  check that a non-matching hasCrossSiteAncestor does not return a
          //  cookie
          chrome.cookies.getAll(
              {
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite,
                  hasCrossSiteAncestor: true
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(0, cookies.length);
              }));
          // Confirm that cookie.getAll() will return an error for an invalid
          // value for the hasCrossSiteAncestor with no error.
          chrome.cookies.getAll(
              {partitionKey: {topLevelSite: '', hasCrossSiteAncestor: true}},
              fail(
                  'partitionKey with empty topLevelSite unexpectedly has a ' +
                  'cross-site ancestor value of true.'));

          // check that a matching hasCrossSiteAncestor gets cookie
          chrome.cookies.getAll(
              {
                partitionKey: {
                  topLevelSite: TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                                    .topLevelSite,
                  hasCrossSiteAncestor: TEST_FIRST_PARTY_PARTITIONED_COOKIE
                                            .partitionKey.hasCrossSiteAncestor
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    cookies[0].partitionKey.hasCrossSiteAncestor,
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.partitionKey
                        .hasCrossSiteAncestor);
                chrome.test.assertEq(
                    TEST_FIRST_PARTY_PARTITIONED_COOKIE.name, cookies[0].name);
              }));
        }))
  },
  function getPartitionedCookie() {
    removeTestCookies();
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE, pass(function() {
          // No partitionKey doesn't work because there is a partitionKey
          // present for the cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
              },
              pass(expectNullCookie));
          // Empty topLevelSite doesn't work because it's matches an
          // unpartitioned cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {topLevelSite: '', hasCrossSiteAncestor: false}
              },
              pass(expectNullCookie));
          // Invalid partitionKey results in an error.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {topLevelSite: '', hasCrossSiteAncestor: true}
              },
              fail(
                  'partitionKey with empty topLevelSite unexpectedly has a ' +
                  'cross-site ancestor value of true.'));
          // Check that different topLevelSite results in no cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite: TEST_PARTITIONED_COOKIE.url,
                  hasCrossSiteAncestor:
                      TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor
                }
              },
              pass(expectNullCookie));
          // Check that different hasCrossSiteAncestor results in no cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
                  hasCrossSiteAncestor:
                      !TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor
                }
              },
              pass(expectNullCookie));
          // Confirm no cookie retrieved with opaque site.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite: TEST_OPAQUE_URL,
                  hasCrossSiteAncestor:
                      TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor
                }
              },
              pass(expectNullCookie));
          // Confirm that cookie.get() will accept an invalid value for the
          // hasCrossSiteAncestor with no error.
          chrome.cookies.get(
              {
                url: TEST_FIRST_PARTY_PARTITIONED_COOKIE.url,
                name: TEST_FIRST_PARTY_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite: TEST_CROSS_SITE_TOP_LEVEL_SITE,
                  hasCrossSiteAncestor: false
                }
              },
              pass(function(cookie) {
                expectNullCookie(cookie);
              }));
          // Confirm that a cookie can be retrieved with
          // correct parameters.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                chrome.test.assertEq(TEST_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.value, cookie.value);
                chrome.test.assertEq(TEST_HOST, cookie.domain);
                chrome.test.assertEq(true, cookie.hostOnly);
                chrome.test.assertEq('/', cookie.path);
                chrome.test.assertEq(true, cookie.secure);
                chrome.test.assertEq(true, cookie.httpOnly);
                chrome.test.assertEq('unspecified', cookie.sameSite);
                chrome.test.assertEq(false, cookie.session);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
                    cookie.partitionKey.topLevelSite);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor,
                    cookie.partitionKey.hasCrossSiteAncestor);
              }));
        }));
    // confirm that a partition key object can be passed to get
    chrome.cookies.get(
        {
          url: TEST_PARTITIONED_COOKIE.url,
          name: TEST_PARTITIONED_COOKIE.name,
          partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
        }));
    // Confirm that a invalid topLevelSite in partitionKey will result in a
    // throw.
    chrome.cookies.get(
        {
          url: TEST_PARTITIONED_COOKIE.url,
          name: TEST_PARTITIONED_COOKIE.name,
          partitionKey: TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE.partitionKey
        },
        fail('Cannot deserialize opaque origin to CookiePartitionKey'));
  },
  function setPartitionedCookie() {
    removeTestCookies();
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE, pass(function() {
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                // Confirm that the name and the top-level site that are set get
                // returned.
                chrome.test.assertEq(TEST_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
                    cookie.partitionKey.topLevelSite);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor,
                    cookie.partitionKey.hasCrossSiteAncestor);
              }));

          // Confirm that setting a cookie with an opaque top_level_site,
          // does not work but also does not crash.
          chrome.cookies.set(
              TEST_PARTITIONED_COOKIE_OPAQUE_TOP_LEVEL_SITE,
              fail(
                  'Failed to parse or set cookie named ' +
                  '"OPAQUE_TOP_LEVEL_SITE".'));

          // Confirm that trying to set a partitioned cookie that does not have
          // secure property equal true will result in a fail.
          chrome.cookies.set(
              TEST_PARTITIONED_COOKIE_SECURE_FALSE,
              fail('Failed to parse or set cookie named "SECURE_FALSE".'));

          // Confirm that a invalid partition key will result in a throw.
          chrome.cookies.set(
              TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE,
              fail('Invalid value for CookiePartitionKey.topLevelSite.'));
          // Confirm that cookie.set() will throw an error when asked to set an
          // invalid value for the hasCrossSiteAncestor.
          chrome.cookies.set(
              {
                url: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.url,
                name: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.name,
                partitionKey: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR
                                  .partitionKey
              },
              fail(
                  'partitionKey has a first party value for ' +
                  'hasCrossSiteAncestor when the url and the ' +
                  'topLevelSite are not first party.'));
        }))

    // Confirm that setting a cookie with an emptyKey results in an
    // unpartitioned cookie being set.
    let emptyKey = structuredClone(TEST_PARTITIONED_COOKIE);
    emptyKey.partitionKey = {};
    emptyKey.value = 'emptyValue';
    chrome.cookies.set(emptyKey, pass(function(cookie) {
                         chrome.test.assertEq(
                             cookie.value,
                             emptyKey.value,
                         );
                         chrome.test.assertEq(
                             cookie.partitionKey,
                             TEST_UNPARTITIONED_COOKIE.partitionKey);
                         chrome.cookies.remove(
                             {name: emptyKey.name, url: emptyKey.url});
                       }));

    // Confirm that setting a cookie with an no `hasCrossSiteAncestor` but a
    // `url` and `toplevel` site that are third party results in the value being
    // correctly populated by the browser.
    const thirdPartyAncestor = structuredClone(TEST_PARTITIONED_COOKIE);
    thirdPartyAncestor.partitionKey = {
      topLevelSite: TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
    };
    thirdPartyAncestor.value = 'thirdPartyAncestor';
    thirdPartyAncestor.partitionKey = {topLevelSite: 'https://notcookies.com'};
    chrome.cookies.set(
        thirdPartyAncestor, pass(function(cookie) {
          chrome.test.assertEq(
              cookie.value,
              thirdPartyAncestor.value,
          );
          const expectedKey = {
            topLevelSite: thirdPartyAncestor.partitionKey.topLevelSite,
            hasCrossSiteAncestor: true
          };
          chrome.test.assertEq(cookie.partitionKey, expectedKey);
          chrome.cookies.remove({
            name: thirdPartyAncestor.name,
            url: thirdPartyAncestor.url,
            partitionKey: expectedKey
          });
        }));

    // Confirm that setting a cookie with no `hasCrossSiteAncestor` but a `url`
    // and `toplevel` site that are first party, results in the value being
    // correctly populated by the browser.
    const firstPartyAncestor =
        structuredClone(TEST_FIRST_PARTY_PARTITIONED_COOKIE);
    firstPartyAncestor.value = 'firstPartyAncestor';
    chrome.cookies.set(
        firstPartyAncestor, pass(function(cookie) {
          chrome.test.assertEq(
              cookie.value,
              firstPartyAncestor.value,
          );
          const expectedKey = {
            topLevelSite: firstPartyAncestor.partitionKey.topLevelSite,
            hasCrossSiteAncestor: false
          };
          chrome.test.assertEq(cookie.partitionKey, expectedKey);
          chrome.cookies.remove({
            name: firstPartyAncestor.name,
            url: firstPartyAncestor.url,
            partitionKey: expectedKey
          });
        }));

    // Confirm that setting a partitioned cookie with an invalid `partitionKey`
    // that contains a `hasCrossSiteAncestor` value but no `topLevelSite`
    // results in an error.
    const invalid = structuredClone(TEST_PARTITIONED_COOKIE);
    invalid.partitionKey = {hasCrossSiteAncestor: false};
    chrome.cookies.set(
        invalid,
        fail(
            'CookiePartitionKey.topLevelSite is not present when ' +
            'CookiePartitionKey.hasCrossSiteAncestor is present.'));
    // Confirm that value of `hasCrossSiteAncestor` doesn't impact return with
    // an invalid `partitionKey`
    invalid.partitionKey = {hasCrossSiteAncestor: true};
    chrome.cookies.set(
        invalid,
        fail(
            'CookiePartitionKey.topLevelSite is not present when ' +
            'CookiePartitionKey.hasCrossSiteAncestor is present.'));
  },
  function getAllPartitionedCookies() {
    removeTestCookies();
    // Confirm that no cookies are present.
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    // Set a partitioned cookie and retrieve it.
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE, pass(function() {
          chrome.cookies.getAll(
              {partitionKey: TEST_PARTITIONED_COOKIE.partitionKey},
              pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
              }));
          // Set another cookie with the same partition key as previous cookie
          // return both cookies by passing the common partition key.
          chrome.cookies.set(TEST_PARTITIONED_COOKIE_SAME_KEY);
          chrome.cookies.getAll(
              {partitionKey: TEST_PARTITIONED_COOKIE.partitionKey},
              pass(function(cookies) {
                chrome.test.assertEq(2, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
              },
              pass(function(cookies) {
                chrome.test.assertEq(2, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
              }));
          // Set cookie with a different partition key than the previous
          // cookies. Ensure that only cookies with the original partition key
          // are returned.
          chrome.cookies.set(TEST_PARTITIONED_COOKIE_DIFFERENT_KEY);
          chrome.cookies.getAll(
              {partitionKey: TEST_PARTITIONED_COOKIE.partitionKey},
              pass(function(cookies) {
                chrome.test.assertEq(2, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(2, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
              }));
          // Set another unpartitioned cookie and check that only the previous
          // cookies with the common partition key are returned.
          chrome.cookies.set(TEST_SECURE_COOKIE);
          chrome.cookies.getAll(
              {
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
                }
              },
              pass(function(cookies) {
                chrome.test.assertEq(2, cookies.length);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
            {
              url: TEST_PARTITIONED_COOKIE.url,
              partitionKey: {
                topLevelSite:
                    TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
              }
            },
            pass(function(cookies) {
              chrome.test.assertEq(2, cookies.length);
              chrome.test.assertEq(
                  TEST_PARTITIONED_COOKIE.name, cookies[0].name);
              chrome.test.assertEq(
                  TEST_PARTITIONED_COOKIE_SAME_KEY.name, cookies[1].name);
            }));
          // Set another cookie with a name common to the previous cookie but
          // with a partition key. Confirm that the getAll call only returns
          // the cookie with no partition key
          chrome.cookies.set(TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME);
          chrome.cookies.getAll(
              {name: TEST_SECURE_COOKIE.name}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(null, cookies[0].partitionKey);
              }));

          // Confirm that passing an empty top level site, returns
          // cookies with and without partition keys.
          chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                                  chrome.test.assertEq(5, cookies.length);
                                }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
              {url: TEST_PARTITIONED_COOKIE.url, partitionKey: {}},
              pass(function(cookies) {
                chrome.test.assertEq(5, cookies.length);
              }));

          // Confirm that passing an undefined top level site, returns
          // cookies with and without partition keys.
          chrome.cookies.getAll(
              {partitionKey: {topLevelSite: undefined}},
              pass(function(cookies) {
                chrome.test.assertEq(5, cookies.length);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
            {
              url: TEST_PARTITIONED_COOKIE.url,
              partitionKey: {topLevelSite: undefined}
            },
            pass(function(cookies) {
              chrome.test.assertEq(5, cookies.length);
            }));

          // Confirm that passing an empty string for top level site,
          // returns unpartitioned cookies.
          chrome.cookies.getAll(
              {partitionKey: {topLevelSite: ''}}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(null, cookies[0].partitionKey);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
            {
              url: TEST_PARTITIONED_COOKIE.url,
              partitionKey: {topLevelSite: ''}
            },
            pass(function(cookies) {
              chrome.test.assertEq(1, cookies.length);
              chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
              chrome.test.assertEq(null, cookies[0].partitionKey);
            }));

          // Confirm that passing an empty object will only get
          // cookies with no partition key.
          chrome.cookies.getAll(
              {}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(null, cookies[0].partitionKey);
              }));
          // Try supplying a URL, which causes implementation to take
          // a different code path.
          chrome.cookies.getAll(
            {url: TEST_PARTITIONED_COOKIE.url}, pass(function(cookies) {
              chrome.test.assertEq(1, cookies.length);
              chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
              chrome.test.assertEq(null, cookies[0].partitionKey);
            }));

          // Confirm callback fails when an invalid topLevelSite is present in
          // the partitionKey.
          chrome.cookies.getAll(
              {
                partitionKey:
                    TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE.partitionKey
              },
              fail('Cannot deserialize opaque origin to CookiePartitionKey'));
        }));
    // Confirm that cookie.getAll() will accept an invalid value for the
    // hasCrossSiteAncestor with no error.
    chrome.cookies.getAll(
        {
          url: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.url,
          partitionKey:
              TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.partitionKey
        },
        pass(function(cookies) {
          chrome.test.assertEq(0, cookies.length);
        }));
  },
  function removePartitionedCookieWithHasCrossSiteAncestor() {
    removeTestCookies();
    // Confirm that no cookies are present.
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    // Set partitioned cookie and confirm that it is present.
    chrome.cookies.set(TEST_PARTITIONED_COOKIE);
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));
    // Removing cookie with incorrect hasCrossSiteAncestor value does not remove
    // cookie.
    chrome.cookies.remove({
      name: TEST_PARTITIONED_COOKIE.name,
      url: TEST_PARTITIONED_COOKIE.url,
      partitionKey: {
        topLevelSite: TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
        hasCrossSiteAncestor:
            !TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor,
      }
    })
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));
    // Removing cookie with no topLevelSite but a correct hasCrossSiteAncestor
    // value does not remove cookie.
    chrome.cookies.remove(
        {
          name: TEST_PARTITIONED_COOKIE.name,
          url: TEST_PARTITIONED_COOKIE.url,
          partitionKey: {
            hasCrossSiteAncestor:
                TEST_PARTITIONED_COOKIE.partitionKey.hasCrossSiteAncestor,
          }
        },
        fail('CookiePartitionKey.topLevelSite unexpectedly not present.'));
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));

    // Confirm that cookie.remove() will accept an invalid value for the
    // hasCrossSiteAncestor with no error but won't remove the cookie.
    chrome.cookies.remove({
      name: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.name,
      url: TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.url,
      partitionKey:
          TEST_PARTITIONED_INVALID_HAS_CROSS_SITE_ANCESTOR.partitionKey
    });
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));
    // Removing cookie with no hasCrossSiteAncestor results in removal.
    chrome.cookies.remove({
      name: TEST_PARTITIONED_COOKIE.name,
      url: TEST_PARTITIONED_COOKIE.url,
      partitionKey: {
        topLevelSite: TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
      }
    })
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
  },
  function removePartitionedCookie() {
    removeTestCookies();
    // Confirm that no cookies are present.
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    // Set partitioned cookie and confirm that it's present.
    chrome.cookies.set(TEST_PARTITIONED_COOKIE);
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));
    // Remove partitioned cookie by populating the partition key object manually
    // and confirm that it's gone.
    chrome.cookies.remove({
      url: TEST_PARTITIONED_COOKIE.url,
      name: TEST_PARTITIONED_COOKIE.name,
      partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
    })
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    // Set partitioned cookie and confirm that it's present.
    chrome.cookies.set(TEST_PARTITIONED_COOKIE);
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(1, cookies.length);
                          }));
    // Remove partitioned cookie by passing an existing partition key object and
    // confirm that it's gone.
    chrome.cookies.remove({
      url: TEST_PARTITIONED_COOKIE.url,
      name: TEST_PARTITIONED_COOKIE.name,
      partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
    })
    chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                            chrome.test.assertEq(0, cookies.length);
                          }));
    // Confirm that a invalid topLevelSite in partitionKey will result in a
    // error.
    chrome.cookies.remove(
        {
          url: TEST_PARTITIONED_COOKIE.url,
          name: TEST_PARTITIONED_COOKIE.name,
          partitionKey: TEST_PARTITIONED_INVALID_TOP_LEVEL_SITE.partitionKey
        },
        fail('Cannot deserialize opaque origin to CookiePartitionKey'));
  },
  function setOddCookies() {
    removeTestCookies();
    // URL lacking permissions.
    chrome.cookies.set(
        {url: TEST_UNPERMITTED_URL, name: 'abcd', domain: TEST_DOMAIN},
        fail('No host permissions for cookies at url: "' +
            TEST_UNPERMITTED_URL + '".',
            function () {
              chrome.cookies.get({url: TEST_URL, name: 'abcd'},
                pass(expectNullCookie));
            }));
    // Attribute values containing invalid characters are disallowed.
    chrome.cookies.set({url: TEST_URL, name: 'abcd=efg'}, fail(
        'Failed to parse or set cookie named "abcd=efg".',
        function() {
          chrome.cookies.get({url: TEST_URL, name: 'abcd'},
              pass(expectNullCookie));
        }));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', value: 'HI;LO'},
        fail(
          'Failed to parse or set cookie named "abcd".',
          function () {
            chrome.cookies.get({url: TEST_URL, name: 'abcd'},
                pass(expectNullCookie));
          }));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', domain: 'cookies.com\r'},
        fail(
          'Failed to parse or set cookie named "abcd".',
          function () {
            chrome.cookies.get({url: TEST_URL, name: 'abcd'},
                pass(expectNullCookie));
          }));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', domain: 'somedomain.com'},
        fail(
          'Failed to parse or set cookie named "abcd".',
          function () {
            chrome.cookies.get({url: TEST_URL, name: 'abcd'},
                pass(expectNullCookie));
          }));
    chrome.cookies.set({
      url: TEST_ODD_URL,
      name: 'abcd',
      domain: TEST_ODD_DOMAIN,
      path: TEST_ODD_PATH
    }, pass(function () {
      chrome.cookies.get({url: TEST_ODD_URL, name: 'abcd'},
          pass(function(cookie) {
            expectValidCookie(cookie);
            chrome.test.assertEq(TEST_ODD_DOMAIN, unescape(cookie.domain));
            chrome.test.assertEq(TEST_ODD_PATH, unescape(cookie.path));
          }));
    }));
  },
  function setSameSiteCookies() {
    removeTestCookies();

    // Property is left out
    chrome.cookies.set(
      {url: TEST_URL, name: "AA", value: "1"},
      pass(function () {
        chrome.cookies.get({url: TEST_URL, name: "AA"}, pass(function (c) {
          expectValidCookie(c);
          chrome.test.assertEq('unspecified', c.sameSite);
        }));
      }));

    // No same-site restriction
    chrome.cookies.set(
      {url: TEST_URL_HTTPS, name: "A", value: "1", sameSite: "no_restriction",
       secure: true},
      pass(function () {
        chrome.cookies.get({url: TEST_URL_HTTPS, name: "A"}, pass(function (c) {
          expectValidCookie(c);
          chrome.test.assertEq("no_restriction", c.sameSite);
        }));
      }));

    // Lax same-site restriction
    chrome.cookies.set(
      {url: TEST_URL, name: "B", value: "1", sameSite: "lax"},
      pass(function () {
        chrome.cookies.get({url: TEST_URL, name: "B"}, pass(function (c) {
          expectValidCookie(c);
          chrome.test.assertEq("lax", c.sameSite);
        }));
      }));

    // Strict same-site restriction
    chrome.cookies.set(
      {url: TEST_URL, name: "C", value: "1", sameSite: "strict"},
      pass(function () {
        chrome.cookies.get({url: TEST_URL, name: "C"}, pass(function (c) {
          expectValidCookie(c);
          chrome.test.assertEq("strict", c.sameSite);
        }));
      }));

    // Unspecified
    chrome.cookies.set(
      {url: TEST_URL, name: "D", value: "1", sameSite: "unspecified"},
      pass(function () {
        chrome.cookies.get({url: TEST_URL, name: "D"}, pass(function (c) {
          expectValidCookie(c);
          chrome.test.assertEq('unspecified', c.sameSite);
        }));
      }));
  },
  function setSameSiteCookiesInsecureNone() {
    chrome.test.getConfig(config => {
      // No same-site restriction but also not secure. This should succeed if
      // and only if the feature requiring SameSite=none cookies to be secure
      // cookies is disabled.
      var sameSiteNoneRequiresSecure = config.customArg === 'true';
      removeTestCookies();

      chrome.cookies.set(
        {url: TEST_URL, name: "AI", value: "1", sameSite: "no_restriction"},
        setResult => {
          if (sameSiteNoneRequiresSecure) {
            chrome.test.assertLastError(
                 'Failed to parse or set cookie named "AI".');
            chrome.test.assertEq(null, setResult);
            chrome.test.succeed();
          } else {
            chrome.test.assertNoLastError();
            chrome.cookies.get(
                {url: TEST_URL_HTTPS, name: "AI"},
                pass(function (c) {
                  expectValidCookie(c);
                  chrome.test.assertEq("no_restriction", c.sameSite);
                }));
          }
        });
    });
  },
  function setCookiesWithCallbacks() {
    removeTestCookies();
    // Basics.
    chrome.cookies.set(
        TEST_BASIC_COOKIE,
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookie.name);
          chrome.test.assertEq(TEST_BASIC_COOKIE.value, cookie.value);
          chrome.test.assertEq(TEST_HOST, cookie.domain);
          chrome.test.assertEq(true, cookie.hostOnly);
          chrome.test.assertEq('/', cookie.path);
          chrome.test.assertEq(false, cookie.secure);
          chrome.test.assertEq(false, cookie.httpOnly);
          chrome.test.assertEq(true, cookie.session);
          chrome.test.assertTrue(typeof cookie.expirationDate === 'undefined',
              'Session cookie should not have expirationDate property.');
          chrome.test.assertTrue(typeof cookie.storeId !== 'undefined',
                                 'Cookie store ID not provided.');
        }));
    // Invalid values generate callback with no arguments, and error messages
    chrome.cookies.set(
        {url: TEST_UNPERMITTED_URL, name: 'abcd', domain: TEST_DOMAIN},
        fail(
            'No host permissions for cookies at url: "'
                + TEST_UNPERMITTED_URL + '".',
             expectUndefinedCookie));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd=efg'},
        fail('Failed to parse or set cookie named "abcd=efg".',
             expectUndefinedCookie));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', value: 'HI;LO'},
        fail('Failed to parse or set cookie named "abcd".',
             expectUndefinedCookie));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', domain: 'cookies.com\r'},
        fail('Failed to parse or set cookie named "abcd".',
             expectUndefinedCookie));
    chrome.cookies.set(
        {url: TEST_URL, name: 'abcd', domain: 'somedomain.com'},
        fail('Failed to parse or set cookie named "abcd".',
            expectUndefinedCookie));
    // Expired cookies generate callback with "null" cookie
    chrome.cookies.set(TEST_BASIC_EXPIRED_COOKIE, pass(expectUndefinedCookie));
    // Odd (but valid!) URLs get callbacks too!
    chrome.cookies.set(
        {
          url: TEST_ODD_URL,
          name: 'abcd',
          domain: TEST_ODD_DOMAIN,
          path: TEST_ODD_PATH
        },
        pass(function(cookie) {
          expectValidCookie(cookie);
          chrome.test.assertEq(TEST_ODD_DOMAIN, unescape(cookie.domain));
          chrome.test.assertEq(TEST_ODD_PATH, unescape(cookie.path));
        }));
  },
  function removeCookie() {
    removeTestCookies();
    chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
      chrome.cookies.get(
          {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
          pass(function (c) {
            expectValidCookie(c);

            // Removal with any domain-matching URL will do.
            chrome.cookies.remove(
                {url: TEST_URL4, name: TEST_BASIC_COOKIE.name},
                pass(function () {
                  chrome.cookies.get(
                      {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
                      pass(expectNullCookie));
                  }));
          }));
    }));
    // Set with an expired date should also remove the cookie.
    chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
      chrome.cookies.get(
          {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
          pass(expectValidCookie));
      chrome.cookies.set(TEST_BASIC_EXPIRED_COOKIE, pass(function () {
        chrome.cookies.get(
            {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
            pass(expectNullCookie));
      }));
    }));
    // Removal with a disallowed URL shouldn't do anything.
    chrome.cookies.set(TEST_DOMAIN_COOKIE, pass(function () {
      chrome.cookies.get(
          {url: TEST_URL2, name: TEST_DOMAIN_COOKIE.name},
          pass(function (c) {
            expectValidCookie(c);
            chrome.cookies.remove(
              {url: TEST_UNPERMITTED_URL, name: TEST_DOMAIN_COOKIE.name},
              fail(
                  'No host permissions for cookies at url: "'
                      + TEST_UNPERMITTED_URL + '".',
                  function () {
                  chrome.cookies.get(
                      {url: TEST_URL2, name: TEST_DOMAIN_COOKIE.name},
                      pass(expectValidCookie));
                  }));
              }));
    }));
  },
  function removeCookiesWithCallbacks() {
    removeTestCookies();
    chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
      chrome.cookies.get(
          {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
          pass(expectValidCookie));
      // Removal with any domain-matching URL will trigger callback with the
      // removed cookie's "url" and "name" fields.
      chrome.cookies.remove(
          {url: TEST_URL4, name: TEST_BASIC_COOKIE.name},
          pass(function(data) {
            chrome.test.assertEq(TEST_URL4, data.url);
            chrome.test.assertEq(TEST_BASIC_COOKIE.name, data.name);
            chrome.test.assertTrue(typeof data.storeId !== 'undefined',
                                   'Cookie store ID not provided.');
          }));
    }));
    // Removal with a disallowed URL should trigger the callback with no
    // arguments, and a set error message.
    chrome.cookies.set(TEST_DOMAIN_COOKIE, pass(function () {
      chrome.cookies.remove(
          {url: TEST_UNPERMITTED_URL, name: TEST_DOMAIN_COOKIE.name},
          fail(
              'No host permissions for cookies at url: "'
                + TEST_UNPERMITTED_URL + '".',
              expectUndefinedCookie));
    }));
  },
  function getAllCookies() {
    removeTestCookies();
    chrome.cookies.getAll({}, pass(function(cookies) {
      chrome.test.assertEq(0, cookies.length);
    }));
    chrome.cookies.set(
        TEST_BASIC_COOKIE, pass(function() {
          chrome.cookies.set(
              TEST_SECURE_COOKIE, pass(function() {
                chrome.cookies.getAll(
                    {domain: TEST_DOMAIN}, pass(function(cookies) {
                      chrome.test.assertEq(2, cookies.length);
                      chrome.test.assertEq(
                          TEST_SECURE_COOKIE.name, cookies[0].name);
                      chrome.test.assertEq(
                          TEST_BASIC_COOKIE.name, cookies[1].name);
                    }));
                chrome.cookies.getAll(
                    {name: TEST_BASIC_COOKIE.name}, pass(function(cookies) {
                      chrome.test.assertEq(1, cookies.length);
                      chrome.test.assertEq(
                          TEST_BASIC_COOKIE.name, cookies[0].name);
                    }));
                chrome.cookies.getAll({secure: true}, pass(function(cookies) {
                                        chrome.test.assertEq(1, cookies.length);
                                        chrome.test.assertEq(
                                            TEST_SECURE_COOKIE.name,
                                            cookies[0].name);
                                      }));
                chrome.cookies.getAll(
                    {url: 'invalid url'}, fail('Invalid url: "invalid url".'));
                chrome.cookies.getAll(
                    {
                      url: TEST_URL,
                    },
                    pass(function(cookies) {
                      chrome.test.assertEq(1, cookies.length);
                      chrome.test.assertEq(
                          TEST_BASIC_COOKIE.name, cookies[0].name);
                    }));
              }));
        }));
  },
  function getAllCookieStores() {
    removeTestCookies();
    chrome.cookies.getAllCookieStores(
        pass(function(cookieStores) {
          chrome.test.assertEq(1, cookieStores.length);
          chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
            chrome.cookies.get(
                {url: TEST_URL, name: TEST_BASIC_COOKIE.name},
                pass(function(cookie) {
                  chrome.test.assertEq(cookieStores[0].id, cookie.storeId);
                }));
            chrome.cookies.getAll(
                {storeId: cookieStores[0].id},
                pass(function(cookies) {
                  chrome.test.assertEq(1, cookies.length);
                  chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[0].name);
                }));
          }));
        }));
  }
]);
