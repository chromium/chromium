// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SharedDictionaryView} from 'chrome://net-internals/shared_dictionary_view.js';
import {$} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {switchToView} from './test_util.js';

suite('NetInternalsSharedDictionaryViewTest', function() {
  function checkOutput() {
    return new Promise(resolve => {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      if (elementToObserve.textContent !== '') {
        resolve(elementToObserve.textContent);
        return;
      }
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          resolve(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
    });
  }

  function setNetworkContext() {
    chrome.send('setNetworkContextForTesting');
  }

  function resetNetworkContext() {
    chrome.send('resetNetworkContextForTesting');
  }

  function registerTestSharedDictionary(dictionary) {
    chrome.send('registerTestSharedDictionary', [JSON.stringify(dictionary)]);
  }

  function clickReloadAndObserveOutput() {
    return new Promise(resolve => {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          resolve(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      $(SharedDictionaryView.RELOAD_BUTTON_ID).click();
    });
  }

  function clickFirstClearButtonAndObserveOutput() {
    return new Promise(resolve => {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          resolve(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      const className = 'clear-shared-dictionary-button-for-isolation';
      document.getElementsByClassName(className)[0].click();
    });
  }

  function clickClearAllButtonAndObserveOutput() {
    return new Promise(resolve => {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          resolve(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      $(SharedDictionaryView.CLEAR_ALL_BUTTON_ID).click();
    });
  }

  /**
   * Reloading without any dictionary.
   */
  test('ReloadEmpty', async function() {
    switchToView('sharedDictionary');
    let result = await checkOutput();
    assertEquals('no data', result);
    setNetworkContext();
    result = await clickReloadAndObserveOutput();
    assertEquals('no data', result);
    resetNetworkContext();
  });

  /**
   * Reloading after registering one dictionary.
   */
  test('ReloadOneDictionary', async function() {
    switchToView('sharedDictionary');
    let result = await checkOutput();
    assertEquals('no data', result);
    setNetworkContext();
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p*',
      match_dest: ['', 'document'],
      id: 'test_dictionary_id',
      dictionary_url: 'https://d.test/d',
      last_fetch_time: '3 Jul 2023 13:01 GMT',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    });
    result = await clickReloadAndObserveOutput();
    assertEquals(
        'Isolation key : {frame_origin: https://a.test, ' +
            'top_frame_site: https://b.test}' +
            'Total usage: 123 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d",\n' +
            '    "expiration": "1000",\n' +
            '    "hash": "' +
            '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
            '",\n' +
            '    "id": "test_dictionary_id",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 13:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p*",\n' +
            '    "match_dest": [\n' +
            '      "",\n' +
            '      "document"\n' +
            '    ],\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  }\n' +
            ']',
        result);
    resetNetworkContext();
  });

  /**
   * Reloading after registering two dictionaries.
   */
  test(
      'ReloadTwoDictionaries', async function() {
        switchToView('sharedDictionary');
        let result = await checkOutput();
        assertEquals('no data', result);
        setNetworkContext();
        registerTestSharedDictionary({
          frame_origin: 'https://a.test',
          top_frame_site: 'https://b.test',
          match: '/p1*',
          dictionary_url: 'https://d.test/d1',
          last_fetch_time: '3 Jul 2023 13:01 GMT',
          response_time: '3 Jul 2023 13:00 GMT',
          expiration: 1000,
          last_used_time: '3 Jul 2023 14:00 GMT',
          size: 123,
          hash:
              '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
        });
        registerTestSharedDictionary({
          frame_origin: 'https://a.test',
          top_frame_site: 'https://b.test',
          match: '/p2*',
          dictionary_url: 'https://d.test/d2',
          last_fetch_time: '3 Jul 2023 15:01 GMT',
          response_time: '3 Jul 2023 15:00 GMT',
          expiration: 2000,
          last_used_time: '3 Jul 2023 16:00 GMT',
          size: 234,
          hash:
              '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
        });
        result = await clickReloadAndObserveOutput();
        assertEquals(
            'Isolation key : {frame_origin: https://a.test, ' +
                'top_frame_site: https://b.test}' +
                'Total usage: 357 bytes' +
                'Clear' +
                '[\n' +
                '  {\n' +
                '    "dictionary_url": "https://d.test/d1",\n' +
                '    "expiration": "1000",\n' +
                '    "hash": "' +
                '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
                '",\n' +
                '    "id": "",\n' +
                '    "last_fetch_time": "Mon, 03 Jul 2023 13:01:00 GMT",\n' +
                '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
                '    "match": "/p1*",\n' +
                '    "match_dest": [],\n' +
                '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
                '    "size": "123"\n' +
                '  },\n' +
                '  {\n' +
                '    "dictionary_url": "https://d.test/d2",\n' +
                '    "expiration": "2000",\n' +
                '    "hash": "' +
                '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
                '",\n' +
                '    "id": "",\n' +
                '    "last_fetch_time": "Mon, 03 Jul 2023 15:01:00 GMT",\n' +
                '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
                '    "match": "/p2*",\n' +
                '    "match_dest": [],\n' +
                '    "response_time": "Mon, 03 Jul 2023 15:00:00 GMT",\n' +
                '    "size": "234"\n' +
                '  }\n' +
                ']',
            result);
        resetNetworkContext();
      });

  /**
   * Reloading after registering dictionaries on two isolations.
   */
  test('ReloadTwoIsolations', async function() {
    switchToView('sharedDictionary');
    let result = await checkOutput();
    assertEquals('no data', result);
    setNetworkContext();
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      last_fetch_time: '3 Jul 2023 13:01 GMT',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      last_fetch_time: '3 Jul 2023 15:01 GMT',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      last_fetch_time: '3 Jul 2023 16:01 GMT',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    });
    result = await clickReloadAndObserveOutput();
    assertEquals(
        'Isolation key : {frame_origin: https://a.test, ' +
            'top_frame_site: https://b.test}' +
            'Total usage: 357 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d1",\n' +
            '    "expiration": "1000",\n' +
            '    "hash": "' +
            '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 13:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 15:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 15:00:00 GMT",\n' +
            '    "size": "234"\n' +
            '  }\n' +
            ']' +
            'Isolation key : {frame_origin: https://x.test, ' +
            'top_frame_site: https://y.test}' +
            'Total usage: 345 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d3",\n' +
            '    "expiration": "3000",\n' +
            '    "hash": "' +
            '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 16:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']',
        result);
    resetNetworkContext();
  });

  /**
   * Clear dictionaries for an isolation.
   */
  test('ClearForIsolation', async function() {
    switchToView('sharedDictionary');
    let result = await checkOutput();
    assertEquals('no data', result);
    setNetworkContext();
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      last_fetch_time: '3 Jul 2023 13:01 GMT',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      last_fetch_time: '3 Jul 2023 15:01 GMT',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      last_fetch_time: '3 Jul 2023 16:01 GMT',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    });
    result = await clickReloadAndObserveOutput();
    assertEquals(
        'Isolation key : {frame_origin: https://a.test, ' +
            'top_frame_site: https://b.test}' +
            'Total usage: 357 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d1",\n' +
            '    "expiration": "1000",\n' +
            '    "hash": "' +
            '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 13:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 15:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 15:00:00 GMT",\n' +
            '    "size": "234"\n' +
            '  }\n' +
            ']' +
            'Isolation key : {frame_origin: https://x.test, ' +
            'top_frame_site: https://y.test}' +
            'Total usage: 345 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d3",\n' +
            '    "expiration": "3000",\n' +
            '    "hash": "' +
            '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 16:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']',
        result);
    result = await clickFirstClearButtonAndObserveOutput();
    assertEquals(
        'Isolation key : {frame_origin: https://x.test, ' +
            'top_frame_site: https://y.test}' +
            'Total usage: 345 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d3",\n' +
            '    "expiration": "3000",\n' +
            '    "hash": "' +
            '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 16:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']',
        result);

    result = await clickFirstClearButtonAndObserveOutput();
    assertEquals('no data', result);
    resetNetworkContext();
  });

  /**
   * Clear all dictionaries.
   */
  test('ClearAll', async function() {
    switchToView('sharedDictionary');
    let result = await checkOutput();
    assertEquals('no data', result);
    setNetworkContext();
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      last_fetch_time: '3 Jul 2023 13:01 GMT',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      last_fetch_time: '3 Jul 2023 15:01 GMT',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    });
    registerTestSharedDictionary({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      last_fetch_time: '3 Jul 2023 16:01 GMT',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    });
    result = await clickReloadAndObserveOutput();
    assertEquals(
        'Isolation key : {frame_origin: https://a.test, ' +
            'top_frame_site: https://b.test}' +
            'Total usage: 357 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d1",\n' +
            '    "expiration": "1000",\n' +
            '    "hash": "' +
            '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 13:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 15:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 15:00:00 GMT",\n' +
            '    "size": "234"\n' +
            '  }\n' +
            ']' +
            'Isolation key : {frame_origin: https://x.test, ' +
            'top_frame_site: https://y.test}' +
            'Total usage: 345 bytes' +
            'Clear' +
            '[\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d3",\n' +
            '    "expiration": "3000",\n' +
            '    "hash": "' +
            '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f' +
            '",\n' +
            '    "id": "",\n' +
            '    "last_fetch_time": "Mon, 03 Jul 2023 16:01:00 GMT",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "match_dest": [],\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']',
        result);
    result = await clickClearAllButtonAndObserveOutput();
    assertEquals('no data', result);
    resetNetworkContext();
  });
});
