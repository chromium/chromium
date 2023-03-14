// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!chrome || !chrome.test)
  throw new Error('chrome.test is undefined');

var portNumber;

// This is a good end-to-end test for two reasons. The first is obvious - it
// tests a simple API and makes sure it behaves as expected, as well as testing
// that other APIs are unavailable.
// The second is that chrome.test is itself an extension API, and a rather
// complex one. It requires both traditional bindings (renderer parses args,
// passes info to browser process, browser process does work and responds, re-
// enters JS) and custom JS bindings (in order to have our runTests, assert*
// methods, etc). If any of these stages failed, the test itself would also
// fail.
var tests = [
  function historyApi() {
    chrome.test.assertTrue(!!chrome.history);
    chrome.test.assertTrue(!!chrome.history.TransitionType);
    chrome.test.assertTrue(!!chrome.history.TransitionType.LINK);
    chrome.test.assertTrue(!!chrome.history.TransitionType.TYPED);
    chrome.test.assertTrue(!!chrome.history.getVisits);
    chrome.history.getVisits({url: 'http://example.com'}, function(visits) {
      // We're just testing the bindings, not the history API, so we don't
      // care about the response.
      chrome.test.assertTrue(!!visits);
      chrome.test.succeed();
    });
  },
  function nonexistentApi() {
    chrome.test.assertFalse(!!chrome.nonexistent);
    chrome.test.succeed();
  },
  function disallowedApi() {
    chrome.test.assertFalse(!!chrome.power);
    chrome.test.succeed();
  },
  function overwriteApi() {
    chrome.test.assertTrue(chrome.hasOwnProperty('history'));
    let oldHistory = chrome.history;
    chrome.history = 'foo';
    chrome.test.assertEq('foo', chrome.history);
    delete chrome.history;
    chrome.test.assertFalse(chrome.hasOwnProperty('history'));
    chrome.test.assertEq(undefined, chrome.history);
    chrome.history = oldHistory;
    chrome.test.succeed();
  },
  function events() {
    var createdEvent = new Promise((resolve, reject) => {
      chrome.tabs.onCreated.addListener(tab => {
        resolve(tab.id);
      });
    });
    var createdCallback = new Promise((resolve, reject) => {
      chrome.tabs.create({url: 'http://example.com'}, tab => {
        resolve(tab.id);
      });
    });
    Promise.all([createdEvent, createdCallback]).then(res => {
      chrome.test.assertEq(2, res.length);
      chrome.test.assertEq(res[0], res[1]);
      chrome.test.succeed();
    });
  },
  function testMessaging() {
    var tabId;

    var createPort = function() {
      chrome.test.assertTrue(!!tabId);
      var port = chrome.tabs.connect(tabId);
      chrome.test.assertTrue(!!port, 'Port does not exist');
      port.onMessage.addListener(message => {
        chrome.test.assertEq('content script', message);
        port.disconnect();
        chrome.tabs.sendMessage(tabId, 'async bounce', function(response) {
          chrome.test.assertEq('bounced', response);
          chrome.test.succeed();
        });
      });
      port.postMessage('background page');
    };

    chrome.runtime.onMessage.addListener(function listener(
        message, sender, sendResponse) {
      chrome.test.assertEq('startFlow', message);
      createPort();
      sendResponse('started');
      chrome.runtime.onMessage.removeListener(listener);
    });

    var url = 'http://localhost:' + portNumber +
              '/native_bindings/extension/messaging_test.html';
    chrome.tabs.create({url: url}, function(tab) {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(!!tab);
      chrome.test.assertTrue(!!tab.id && tab.id >= 0);
      tabId = tab.id;
    });
  },
  function injectScript() {
    var url =
        'http://example.com:' + portNumber + '/native_bindings/simple.html';
    // Create a tab, and inject code in it to change its title.
    // chrome.tabs.executeScript relies on external type references
    // (extensionTypes.InjectDetails), so this exercises that flow as well.
    chrome.tabs.create({url: url}, function(tab) {
      chrome.test.assertTrue(!!tab, 'tab');
      // Snag this opportunity to test bindings properties.
      chrome.test.assertTrue(!!chrome.tabs.TAB_ID_NONE);
      chrome.test.assertNe(chrome.tabs.TAB_ID_NONE, tab.id);
      chrome.test.assertEq(new URL(url).host, new URL(tab.pendingUrl).host);
      var code = 'document.title = "new title";';
      chrome.tabs.executeScript(tab.id, {code: code}, function(results) {
        chrome.test.assertTrue(!!results, 'results');
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('new title', results[0]);
        chrome.tabs.get(tab.id, tab => {
          chrome.test.assertEq('new title', tab.title);
          chrome.test.succeed();
        });
      });
    });
  },
  function testLastError() {
    chrome.runtime.setUninstallURL('chrome://newtab', function() {
      var expectedError = 'Invalid URL: "chrome://newtab".';
      chrome.test.assertLastError(expectedError);
      // Explicitly also test the old extension.lastError property.
      chrome.test.assertTrue(!!chrome.extension.lastError);
      chrome.test.assertEq(expectedError, chrome.extension.lastError.message);
      chrome.test.succeed();
    });
  },
  function testStorage() {
    // Check API existence; StorageArea functions.
    chrome.test.assertTrue(!!chrome.storage);
    chrome.test.assertTrue(!!chrome.storage.local, 'no local');
    chrome.test.assertTrue(!!chrome.storage.local.set, 'no set');
    chrome.test.assertTrue(!!chrome.storage.local.get, 'no get');
    chrome.test.assertTrue(!!chrome.storage.local.onChanged, 'no onChanged');
    // Check some properties.
    chrome.test.assertTrue(!!chrome.storage.local.QUOTA_BYTES,
                           'local quota bytes');
    chrome.test.assertFalse(!!chrome.storage.local.MAX_ITEMS,
                            'local max items');
    chrome.test.assertTrue(!!chrome.storage.sync, 'sync');
    chrome.test.assertTrue(!!chrome.storage.sync.QUOTA_BYTES,
                           'sync quota bytes');
    chrome.test.assertTrue(!!chrome.storage.sync.MAX_ITEMS,
                           'sync max items');
    chrome.test.assertTrue(!!chrome.storage.managed, 'managed');
    chrome.test.assertFalse(!!chrome.storage.managed.QUOTA_BYTES,
                            'managed quota bytes');
    chrome.storage.local.set({foo: 'bar', nullkey: null}, () => {
      chrome.storage.local.get(['foo', 'nullkey'], (results) => {
        chrome.test.assertTrue(!!results, 'no results');
        chrome.test.assertTrue(!!results.foo, 'no foo');
        chrome.test.assertEq('bar', results.foo);
        chrome.test.assertTrue('nullkey' in results);
        chrome.test.assertEq(null, results.nullkey);
        chrome.test.succeed();
      });
    });
  },
  function testBrowserActionWithCustomSendRequest() {
    // browserAction.setIcon uses a custom hook that calls sendRequest().
    chrome.browserAction.setIcon({path: 'icon.png'}, chrome.test.succeed);
  },
  function testChromeSetting() {
    chrome.test.assertTrue(!!chrome.privacy, 'privacy');
    chrome.test.assertTrue(!!chrome.privacy.websites, 'websites');
    var cookiePolicy = chrome.privacy.websites.thirdPartyCookiesAllowed;
    chrome.test.assertTrue(!!cookiePolicy, 'cookie policy');
    chrome.test.assertTrue(!!cookiePolicy.get, 'get');
    chrome.test.assertTrue(!!cookiePolicy.set, 'set');
    chrome.test.assertTrue(!!cookiePolicy.clear, 'clear');
    chrome.test.assertTrue(!!cookiePolicy.onChange, 'onchange');

    // The JSON spec for ChromeSettings is weird, because it claims it allows
    // any type for <val> in ChromeSetting.set({value: <val>}), but this is just
    // a hack in our schema generation because we curry in the different types
    // of restrictions. Trying to pass in the wrong type for value should fail
    // (synchronously).
    var caught = false;
    try {
      cookiePolicy.set({value: 'not a bool'});
    } catch (e) {
      caught = true;
    }
    chrome.test.assertTrue(caught, 'caught');

    var listenerPromise = new Promise((resolve, reject) => {
      cookiePolicy.onChange.addListener(function listener(details) {
        chrome.test.assertTrue(!!details, 'listener details');
        chrome.test.assertEq(false, details.value);
        cookiePolicy.onChange.removeListener(listener);
        resolve();
      });
    });

    var methodPromise = new Promise((resolve, reject) => {
      cookiePolicy.get({}, (details) => {
        chrome.test.assertTrue(!!details, 'get details');
        chrome.test.assertTrue(details.value, 'details value true');
        cookiePolicy.set({value: false, scope: 'regular'}, () => {
          cookiePolicy.get({}, (details) => {
            chrome.test.assertTrue(!!details, 'get details');
            chrome.test.assertFalse(details.value, 'details value false');
            resolve();
          });
        });
      });
    });

    Promise.all([listenerPromise, methodPromise]).then(() => {
      chrome.test.succeed();
    });
  },
  function testWebNavigationAndFilteredEvents() {
    // Tests unfiltered events, which can be exercised with the webNavigation
    // API.
    var unfiltered = new Promise((resolve, reject) => {
      var sawSimple1 = false;
      var sawSimple2 = false;
      chrome.webNavigation.onBeforeNavigate.addListener(
          function listener(details) {
        // We create a bunch of tabs in other tests, which can potentially
        // show up here. If this becomes too much of a problem, we can isolate
        // these tests further, but for now, just using a unique url should be
        // sufficient.
        if (details.url.indexOf('unique') == -1)
          return;
        if (details.url.indexOf('simple.html') != -1)
          sawSimple1 = true;
        else if (details.url.indexOf('simple2.html') != -1)
          sawSimple2 = true;
        else
          chrome.test.fail(details.url);

        if (sawSimple1 && sawSimple2) {
          chrome.webNavigation.onBeforeNavigate.removeListener(listener);
          resolve();
        }
      });
    });

    var filtered = new Promise((resolve, reject) => {
      chrome.webNavigation.onBeforeNavigate.addListener(
          function listener(details) {
        chrome.test.assertNe(-1, details.url.indexOf('unique'));
        chrome.test.assertTrue(details.url.indexOf('simple2.html') != -1,
                               details.url);
        chrome.webNavigation.onBeforeNavigate.removeListener(listener);
        resolve();
      }, {url: [{pathContains: 'simple2.html'}]});
    });

    var url1 =
        'http://unique.com:' + portNumber + '/native_bindings/simple.html';
    var url2 =
        'http://unique.com:' + portNumber + '/native_bindings/simple2.html';
    chrome.tabs.create({url: url1});
    chrome.tabs.create({url: url2});

    Promise.all([unfiltered, filtered]).then(() => { chrome.test.succeed(); });
  },
  function testContentSettings() {
    chrome.test.assertTrue(!!chrome.contentSettings);
    chrome.test.assertTrue(!!chrome.contentSettings.javascript);
    var jsPolicy = chrome.contentSettings.javascript;
    chrome.test.assertTrue(!!jsPolicy.get);
    chrome.test.assertTrue(!!jsPolicy.set);
    chrome.test.assertTrue(!!jsPolicy.clear);
    chrome.test.assertTrue(!!jsPolicy.getResourceIdentifiers);

    // Like ChromeSettings above, the JSON spec for ContentSettings claims it
    // allows any type for <val> in ChromeSetting.set({value: <val>}), but this
    // is just a hack in our schema generation because we curry in the different
    // types of restrictions. Trying to pass in the wrong type for value should
    // fail (synchronously).
    var caught = false;
    var url = 'http://example.com/path';
    var pattern = 'http://example.com/*';
    try {
      jsPolicy.set({primaryPattern: pattern, value: 'invalid'});
    } catch (e) {
      caught = true;
    }
    chrome.test.assertTrue(caught);

    var normalSettingTest = new Promise(function(resolve, reject) {
      jsPolicy.get({primaryUrl: url}, (details) => {
        chrome.test.assertTrue(!!details);
        chrome.test.assertEq('allow', details.setting);
        jsPolicy.set({primaryPattern: pattern, setting: 'block'}, () => {
          jsPolicy.get({primaryUrl: url}, (details) => {
            chrome.test.assertTrue(!!details);
            chrome.test.assertEq('block', details.setting);
            resolve();
          });
        });
      });
    });

    // The fullscreen setting is deprecated.
    var fullscreen = chrome.contentSettings.fullscreen;
    var deprecatedSettingTest = new Promise(function(resolve, reject) {
      fullscreen.get({primaryUrl: url}, (details) => {
        chrome.test.assertTrue(!!details);
        chrome.test.assertEq('allow', details.setting);
        // Trying to set the fullscreen setting to anything but 'allow' should
        // silently fail.
        fullscreen.set({primaryPattern: pattern, setting: 'block'}, () => {
          fullscreen.get({primaryUrl: url}, (details) => {
            chrome.test.assertTrue(!!details);
            chrome.test.assertEq('allow', details.setting);
            resolve();
          });
        });
      });
    });

    Promise.all([normalSettingTest, deprecatedSettingTest]).then(() => {
      chrome.test.succeed();
    });
  },
];

chrome.test.getConfig(config => {
  chrome.test.assertTrue(!!config, 'config does not exist');
  chrome.test.assertTrue(!!config.testServer, 'testServer does not exist');
  portNumber = config.testServer.port;
  chrome.test.runTests(tests);
});
