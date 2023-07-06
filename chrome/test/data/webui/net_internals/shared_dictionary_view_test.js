// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SharedDictionaryView} from 'chrome://net-internals/shared_dictionary_view.js';
import {$} from 'chrome://resources/js/util_ts.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

window.shared_dictionary_view_test = {};
const shared_dictionary_view_test = window.shared_dictionary_view_test;
shared_dictionary_view_test.suiteName = 'NetInternalsSharedDictionaryViewTests';
/** @enum {string} */
shared_dictionary_view_test.TestNames = {
  ReloadEmpty: 'Reload empty result',
  ReloadOneDictionary: 'Reload single dictionary',
  ReloadTwoDictionaries: 'Reload two dictionaries',
  ReloadTwoIsolaitons: 'Reload two isolations',
  ClearForIsolation: 'Clear dictionary for an isolation',
  ClearAll: 'Clear all dictionaries',
};

suite(shared_dictionary_view_test.suiteName, function() {
  class CheckOutputTask extends Task {
    start() {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      if (elementToObserve.textContent !== '') {
        this.setCompleteAsync(true);
        this.onTaskDone(elementToObserve.textContent);
        return;
      }
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          this.onTaskDone(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
    }
  }

  class SetNetworkContextTask extends Task {
    start() {
      chrome.send('setNetworkContextForTesting');
      this.setCompleteAsync(true);
      this.onTaskDone();
    }
  }

  class RegisterTestSharedDictionaryTask extends Task {
    constructor(dictionary) {
      super();
      this.dictionary_ = dictionary;
    }
    start() {
      chrome.send(
          'registerTestSharedDictionary', [JSON.stringify(this.dictionary_)]);
      this.setCompleteAsync(true);
      this.onTaskDone();
    }
  }

  class ResetNetworkContextTask extends Task {
    start() {
      chrome.send('resetNetworkContextForTesting');
      this.setCompleteAsync(true);
      this.onTaskDone();
    }
  }

  class ClickReloadAndObserveOutputTask extends Task {
    start() {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          this.onTaskDone(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      $(SharedDictionaryView.RELOAD_BUTTON_ID).click();
    }
  }

  class ClickFirstClearButtonAndObserveOutputTask extends Task {
    start() {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          this.onTaskDone(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      const className = 'clear-shared-dictionary-button-for-isolation';
      document.getElementsByClassName(className)[0].click();
    }
  }

  class ClickClearAllButtonAndObserveOutputTask extends Task {
    start() {
      const elementToObserve =
          document.getElementById('shared-dictionary-output');
      const observer = new MutationObserver(() => {
        if (elementToObserve.textContent !== '') {
          observer.disconnect();
          this.onTaskDone(elementToObserve.textContent);
        }
      });
      observer.observe(elementToObserve, {childList: true, subtree: true});
      $(SharedDictionaryView.CLEAR_ALL_BUTTON_ID).click();
    }
  }

  /**
   * Reloading without any dictionary.
   */
  test(shared_dictionary_view_test.TestNames.ReloadEmpty, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });

  /**
   * Reloading after registering one dictionary.
   */
  test(shared_dictionary_view_test.TestNames.ReloadOneDictionary, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p*',
      dictionary_url: 'https://d.test/d',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    }));
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  }\n' +
            ']'));
    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });

  /**
   * Reloading after registering two dictionaries.
   */
  test(shared_dictionary_view_test.TestNames.ReloadTwoDictionaries, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    }));
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 15:00:00 GMT",\n' +
            '    "size": "234"\n' +
            '  }\n' +
            ']'));
    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });

  /**
   * Reloading after registering dictionaries on two isolations.
   */
  test(shared_dictionary_view_test.TestNames.ReloadTwoIsolaitons, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    }));
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
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
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']'));
    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });

  /**
   * Clear dictionaries for an isolation.
   */
  test(shared_dictionary_view_test.TestNames.ClearForIsolation, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    }));
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
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
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']'));
    taskQueue.addTask(new ClickFirstClearButtonAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']'));

    taskQueue.addTask(new ClickFirstClearButtonAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));

    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });

  /**
   * Clear all dictionaries.
   */
  test(shared_dictionary_view_test.TestNames.ClearAll, function() {
    switchToView('sharedDictionary');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new CheckOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));
    taskQueue.addTask(new SetNetworkContextTask());
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p1*',
      dictionary_url: 'https://d.test/d1',
      response_time: '3 Jul 2023 13:00 GMT',
      expiration: 1000,
      last_used_time: '3 Jul 2023 14:00 GMT',
      size: 123,
      hash: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://a.test',
      top_frame_site: 'https://b.test',
      match: '/p2*',
      dictionary_url: 'https://d.test/d2',
      response_time: '3 Jul 2023 15:00 GMT',
      expiration: 2000,
      last_used_time: '3 Jul 2023 16:00 GMT',
      size: 234,
      hash: '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f',
    }));
    taskQueue.addTask(new RegisterTestSharedDictionaryTask({
      frame_origin: 'https://x.test',
      top_frame_site: 'https://y.test',
      match: '/p3*',
      dictionary_url: 'https://d.test/d3',
      response_time: '3 Jul 2023 16:00 GMT',
      expiration: 3000,
      last_used_time: '3 Jul 2023 18:00 GMT',
      size: 345,
      hash: '404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f',
    }));
    taskQueue.addTask(new ClickReloadAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
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
            '    "last_used_time": "Mon, 03 Jul 2023 14:00:00 GMT",\n' +
            '    "match": "/p1*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 13:00:00 GMT",\n' +
            '    "size": "123"\n' +
            '  },\n' +
            '  {\n' +
            '    "dictionary_url": "https://d.test/d2",\n' +
            '    "expiration": "2000",\n' +
            '    "hash": "' +
            '202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f' +
            '",\n' +
            '    "last_used_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "match": "/p2*",\n' +
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
            '    "last_used_time": "Mon, 03 Jul 2023 18:00:00 GMT",\n' +
            '    "match": "/p3*",\n' +
            '    "response_time": "Mon, 03 Jul 2023 16:00:00 GMT",\n' +
            '    "size": "345"\n' +
            '  }\n' +
            ']'));
    taskQueue.addTask(new ClickClearAllButtonAndObserveOutputTask());
    taskQueue.addFunctionTask(assertEquals.bind(null, 'no data'));

    taskQueue.addTask(new ResetNetworkContextTask());
    return taskQueue.run();
  });
});
