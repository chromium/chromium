// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_DOMAIN = 'cookies.com';
var TEST_PATH = '/auth';
var TEST_HOST = 'www.chrome_extensions.' + TEST_DOMAIN;
var TEST_URL = 'http://' + TEST_HOST + '/foobar.html?arg=toolbar&param=true';
var TEST_URL_HTTPS = 'https://' + TEST_HOST + '/foobar.html?arg=toolbar&param=true';
var TEST_URL2 = 'http://chromium.' + TEST_DOMAIN + '/index.html';
var TEST_URL3 = 'https://' + TEST_HOST + '/content.html';
var TEST_URL4 = 'https://' + TEST_HOST + TEST_PATH + '/content.html';
var TEST_URL5 = 'http://' + TEST_HOST + TEST_PATH + '/content.html';
var TEST_OPAQUE_URL = 'file://' + TEST_DOMAIN;
var TEST_EXPIRATION_DATE = 12345678900;
var TEST_ODD_DOMAIN_HOST_ONLY = 'strange stuff!!.com';
var TEST_ODD_DOMAIN = '.' + TEST_ODD_DOMAIN_HOST_ONLY;
var TEST_ODD_PATH = '/hello = world';
var TEST_ODD_URL = 'http://' + TEST_ODD_DOMAIN_HOST_ONLY + TEST_ODD_PATH + '/index.html';
var TEST_UNPERMITTED_URL = 'http://illegal.' + TEST_DOMAIN + '/';
var TEST_PARTITION_KEY = 'https://' + TEST_DOMAIN;

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
  url: TEST_PARTITION_KEY,
  name: 'PARTITIONEDCOOKIE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_PARTITION_KEY}
};
var TEST_PARTITIONED_COOKIE_SAME_KEY = {
  url: TEST_PARTITION_KEY,
  name: 'PARTITIONEDCOOKIE_SAME_KEY',
  value: 'partitioned_cookie_same_key',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_PARTITION_KEY}
};
var TEST_PARTITIONED_COOKIE_DIFFERENT_KEY = {
  url: TEST_PARTITION_KEY,
  name: 'PARTITIONED_COOKIE_DIFFERENT_KEY',
  value: 'partitioned_cookie_different_key',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_URL5}
};
var TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME = {
  url: TEST_PARTITION_KEY,
  name: TEST_BASIC_COOKIE.name,
  value: 'TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_PARTITION_KEY}
};
var TEST_PARTITIONED_INVALID_PARTITION_KEY = {
  url: TEST_PARTITION_KEY,
  name: 'TEST_PARTITIONED_INVALID_PARTITION_KEY',
  value: 'TEST_PARTITIONED_INVALID_PARTITION_KEY',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: true,
  httpOnly: true,
  partitionKey: {topLevelSite: 'TEST_PARTITIONED_INVALID_PARTITION_KEY'}
};

var TEST_PARTITIONED_COOKIE_SECURE_FALSE = {
  url: TEST_PARTITION_KEY,
  name: 'SECURE_FALSE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: false,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_PARTITION_KEY}
};

var TEST_PARTITIONED_COOKIE_OPAQUE_TOP_LEVEL_SITE = {
  url: TEST_PARTITION_KEY,
  name: 'OPAQUE_TOP_LEVEL_SITE',
  value: 'partitioned_cookie_val',
  expirationDate: TEST_EXPIRATION_DATE,
  secure: false,
  httpOnly: true,
  partitionKey: {topLevelSite: TEST_OPAQUE_URL}
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
    url: TEST_PARTITIONED_COOKIE.url,
    name: TEST_PARTITIONED_COOKIE.name,
    partitionKey: {topLevelSite: TEST_PARTITION_KEY}
  });
  chrome.cookies.remove({
    url: TEST_PARTITIONED_COOKIE_SAME_KEY.url,
    name: TEST_PARTITIONED_COOKIE_SAME_KEY.name,
    partitionKey: {topLevelSite: TEST_PARTITION_KEY}
  });
  chrome.cookies.remove({
    url: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.url,
    name: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.name,
    partitionKey: {
      topLevelSite:
          TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.partitionKey.topLevelSite
    }
  });
  chrome.cookies.remove({
    url: TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME.url,
    name: TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME.name,
    partitionKey: {
      topLevelSite:
          TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME.partitionKey.topLevelSite
    }
  });
  chrome.cookies.remove({
    url: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.url,
    name: TEST_PARTITIONED_COOKIE_DIFFERENT_KEY.name,
    partitionKey: TEST_PARTITIONED_COOKIE.partitionKey
  });
}
var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

chrome.test.runTests([
  function invalidScheme() {
    // Invalid schemes don't work with the cookie API.
    var ourUrl = chrome.extension.getURL('background.js');
    chrome.cookies.get(
        {url: ourUrl, name: 'a'},
        chrome.test.callbackFail(
            'No host permissions for cookies at url: "' +
              ourUrl + '".'));
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
          // Empty topLevelSite doesn't work because it's different than
          // the partitionKey used for the cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {topLevelSite: ''}
              },
              pass(expectNullCookie));
          // Partition key doesn't work because it's different than the
          // partition key used for the cookie.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {topLevelSite: TEST_URL4}
              },
              pass(expectNullCookie));
          // Confirm no cookie retrieved with opaque site.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {topLevelSite: TEST_OPAQUE_URL}
              },
              pass(expectNullCookie));
          // Confirm that a cookie can be retrieved with
          // correct parameters.
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
                }
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                chrome.test.assertEq(TEST_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.value, cookie.value);
                chrome.test.assertEq(TEST_DOMAIN, cookie.domain);
                chrome.test.assertEq(true, cookie.hostOnly);
                chrome.test.assertEq('/', cookie.path);
                chrome.test.assertEq(true, cookie.secure);
                chrome.test.assertEq(true, cookie.httpOnly);
                chrome.test.assertEq('unspecified', cookie.sameSite);
                chrome.test.assertEq(false, cookie.session);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
                    cookie.partitionKey.topLevelSite);
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
    // confirm that the callback fails when an invalid partition key is passed
    chrome.cookies.get(
        {
          url: TEST_PARTITIONED_COOKIE.url,
          name: TEST_PARTITIONED_COOKIE.name,
          partitionKey: TEST_PARTITIONED_INVALID_PARTITION_KEY.partitionKey
        },
        chrome.test.callbackFail(
            'Invalid format for partitionKey.topLevelSite.'));
  },
  function setPartitionedCookie() {
    removeTestCookies();
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE, pass(function() {
          chrome.cookies.get(
              {
                url: TEST_PARTITIONED_COOKIE.url,
                name: TEST_PARTITIONED_COOKIE.name,
                partitionKey: {
                  topLevelSite:
                      TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite
                }
              },
              pass(function(cookie) {
                expectValidCookie(cookie);
                // Confirm that the name and the top-level site that are set get
                // returned.
                chrome.test.assertEq(TEST_PARTITIONED_COOKIE.name, cookie.name);
                chrome.test.assertEq(
                    TEST_PARTITIONED_COOKIE.partitionKey.topLevelSite,
                    cookie.partitionKey.topLevelSite);
              }));
        }));
    // Confirm that setting a cookie with an opaque top_level_site,
    // does not work but also does not crash.
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE_OPAQUE_TOP_LEVEL_SITE,
        chrome.test.callbackFail(
            'Failed to parse or set cookie named "OPAQUE_TOP_LEVEL_SITE".'));

    // Confirm that trying to set a partitioned cookie that does not have
    // secure property equal true will result in a fail.
    chrome.cookies.set(
        TEST_PARTITIONED_COOKIE_SECURE_FALSE,
        chrome.test.callbackFail(
            'Failed to parse or set cookie named "SECURE_FALSE".'));

    // Confirm that a invalid partition key will result in a throw.
    try {
      chrome.cookies.set(TEST_PARTITIONED_INVALID_PARTITION_KEY);
    } catch (e) {
      chrome.test.fail(e.message);
      chrome.assertEq(
          e.message, 'Invalid format for partitionKey.topLevelSite.');
    }
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
              {
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
          // Set cookie with a different partition key than the previous
          // cookies. Ensure that only cookies with the original partition key
          // are returned.
          chrome.cookies.set(TEST_PARTITIONED_COOKIE_DIFFERENT_KEY);
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
          // Set another unpartitioned cookie and check that only the previous
          // cookies with the common partition key are returned.
          chrome.cookies.set(TEST_BASIC_COOKIE);
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
          // Set another cookie with a name common to the previous cookie but
          // with a partition key. Confirm that the getAll call only returns
          // the cookie with no partition key
          chrome.cookies.set(TEST_PARTITIONED_COOKIE_BASIC_COOKIE_NAME);
          chrome.cookies.getAll(
              {name: TEST_BASIC_COOKIE.name}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(null, cookies[0].partitionKey);
              }));

          // Confirm that passing an empty top level site, returns
          // cookies with and without partition keys.
          chrome.cookies.getAll({partitionKey: {}}, pass(function(cookies) {
                                  chrome.test.assertEq(5, cookies.length);
                                }));

          // Confirm that passing an empty object will only get
          // cookies with no partition key.
          chrome.cookies.getAll(
              {}, pass(function(cookies) {
                chrome.test.assertEq(1, cookies.length);
                chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[0].name);
                chrome.test.assertEq(null, cookies[0].partitionKey);
              }));
          // Confirm callback fails when an invalid partition key is passed
          chrome.cookies.getAll(
              {
                partitionKey:
                    TEST_PARTITIONED_INVALID_PARTITION_KEY.partitionKey
              },
              chrome.test.callbackFail(
                  'Invalid format for partitionKey.topLevelSite.'));
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
      partitionKey: {topLevelSite: TEST_PARTITION_KEY}
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
    // Confirm that a invalid partition key will result in a throw
    try {
      chrome.cookies.remove({
        url: TEST_PARTITIONED_COOKIE.url,
        name: TEST_PARTITIONED_COOKIE.name,
        partitionKey: TEST_PARTITIONED_INVALID_PARTITION_KEY.partitionKey
      });
    } catch (e) {
      chrome.test.fail(e.message);
      chrome.assertEq(
          e.message, 'Cannot deserialize opaque origin to CookiePartitionKey');
    }
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
    chrome.cookies.set({
      url: TEST_ODD_URL,
      name: 'abcd',
      domain: TEST_ODD_DOMAIN,
      path: TEST_ODD_PATH
    }, pass(function(cookie) {
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
    chrome.cookies.set(TEST_BASIC_COOKIE, pass(function () {
      chrome.cookies.set(TEST_SECURE_COOKIE, pass(function () {
        chrome.cookies.getAll(
            {domain: TEST_DOMAIN}, pass(function(cookies) {
          chrome.test.assertEq(2, cookies.length);
          chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
          chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[1].name);
        }));
        chrome.cookies.getAll({
          name: TEST_BASIC_COOKIE.name
        }, pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[0].name);
        }));
        chrome.cookies.getAll({
          secure: true
        }, pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(TEST_SECURE_COOKIE.name, cookies[0].name);
        }));
        chrome.cookies.getAll({
          url: 'invalid url'
        }, fail('Invalid url: "invalid url".'));
        chrome.cookies.getAll({
          url: TEST_URL,
        }, pass(function(cookies) {
          chrome.test.assertEq(1, cookies.length);
          chrome.test.assertEq(TEST_BASIC_COOKIE.name, cookies[0].name);
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
