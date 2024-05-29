// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/autofill_metadata_app.js';
import 'chrome://feedback/strings.m.js';

import type {AutofillMetadataAppElement} from 'chrome://feedback/autofill_metadata_app.js';
import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

const DIALOG_ARGS = {
  formStructures: [],
  lastAutofillEvent: '1234567890',
  triggerFieldSignature: '1234567890',
  triggerFormSignature: '12345678901234567890',
};

suite('AutofillMetadataTest', function() {
  let app: AutofillMetadataAppElement;
  let browserProxy: TestFeedbackBrowserProxy;

  setup(function() {
    browserProxy = new TestFeedbackBrowserProxy();
    browserProxy.setDialogArguments(JSON.stringify(DIALOG_ARGS));
    FeedbackBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('autofill-metadata-app');
    document.body.appendChild(app);
  });

  test('RequestAutofillMetadataTest', function() {
    return browserProxy.whenCalled('getDialogArguments');
  });

  test('AutofillMetadataTitleTest', function() {
    assertEquals(
        loadTimeData.getString('autofillMetadataPageTitle'),
        app.$.title.textContent);
  });

  function hasKey(
      arr: chrome.feedbackPrivate.LogsMapEntry[], key: string): boolean {
    return arr.some((obj) => obj['key'] === key);
  }

  test('Check entries parsed from dialog arguments.', async function() {
    const keyValuePairViewer =
        app.shadowRoot!.querySelector('key-value-pair-viewer');
    await microtasksFinished();
    assertTrue(!!keyValuePairViewer);
    const entries = keyValuePairViewer.entries;
    assertTrue(hasKey(entries, 'form_structures'));
    assertTrue(hasKey(entries, 'last_autofill_event'));
    assertTrue(hasKey(entries, 'trigger_form_signature'));
    assertTrue(hasKey(entries, 'trigger_field_signature'));
  });
});
