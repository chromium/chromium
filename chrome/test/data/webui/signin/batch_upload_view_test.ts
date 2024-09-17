// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://batch-upload/batch_upload.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {BatchUploadAppElement} from 'chrome://batch-upload/batch_upload.js';
import {BatchUploadBrowserProxyImpl} from 'chrome://batch-upload/batch_upload.js';
import type {PageRemote} from 'chrome://batch-upload/batch_upload.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBatchUploadBrowserProxy} from './test_batch_upload_browser_proxy.js';

suite('BatchUploadViewTest', function() {
  let batchUploadApp: BatchUploadAppElement;
  let testBatchUploadProxy: TestBatchUploadBrowserProxy;
  let callbackRouterRemote: PageRemote;

  setup(function() {
    testBatchUploadProxy = new TestBatchUploadBrowserProxy();
    callbackRouterRemote =
        testBatchUploadProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BatchUploadBrowserProxyImpl.setInstance(testBatchUploadProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    batchUploadApp = document.createElement('batch-upload-app');
    document.body.append(batchUploadApp);
    // TODO(b/363204445): should send test data created for the test and replace
    // current dummy data.
    callbackRouterRemote.sendData('1');
    return testBatchUploadProxy.handler.whenCalled('updateViewHeight');
  });

  test('HeaderContent', function() {
    assertTrue(isVisible(batchUploadApp));

    // Header.
    assertTrue(isChildVisible(batchUploadApp, '#header'));
    assertTrue(isChildVisible(batchUploadApp, '#title'));
    assertTrue(isChildVisible(batchUploadApp, '#subtitle'));
    // TODO(b/363204445): check based on the test data.
    assertTrue(
        batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                      '#subtitle')!.textContent!.trim()
            .includes('2 passwords and other items'));

    // Account info.
    assertTrue(isChildVisible(batchUploadApp, '#account-info-row'));
    assertTrue(isChildVisible(batchUploadApp, '#account-icon'));
    assertTrue(isChildVisible(batchUploadApp, '#email'));
    // TODO(b/363204445): check based on the test data.
    assertEquals(
        batchUploadApp.shadowRoot!
            .querySelector<CrButtonElement>(
                '#account-info-row')!.textContent!.trim(),
        'elisa.g.beckett@gmail.com');
  });

  test('ClickSave', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#save-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#save-button')!.click();
    // TODO(b/359797313): when implemented should check a different callback.
    return testBatchUploadProxy.handler.whenCalled('close');
  });

  test('ClickClose', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#close-button'));
    batchUploadApp.shadowRoot!.querySelector<CrButtonElement>(
                                  '#close-button')!.click();
    return testBatchUploadProxy.handler.whenCalled('close');
  });

  test('DataSections', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '#data-sections'));

    const dataSections =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-section');
    // TODO(b/363204445): check based on the test data.
    assertEquals(dataSections.length, 2);
  });

  test('SectionTitles', function() {
    assertTrue(isVisible(batchUploadApp));
    assertTrue(isChildVisible(batchUploadApp, '.data-section-header'));

    const sectionTitles =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-section-title');
    // TODO(b/363204445): check based on the test data.
    assertEquals(sectionTitles.length, 2);

    const firstSectionTitle = sectionTitles[0]!;
    // TODO(b/363204445): check based on the test data of the first section.
    const sectionItemCount = 2;
    // All items are selected by default and should be shown in the title.
    assertTrue(firstSectionTitle.textContent!.trim().includes(
        '(' + sectionItemCount + ')'));

    // Uncheck the first item of the first section.
    const itemCheckboxes =
        batchUploadApp.shadowRoot!.querySelectorAll('.item-checkbox');
    assertGT(itemCheckboxes.length, 0);
    (itemCheckboxes[0] as CrCheckboxElement).checked = false;

    // TODO(b/359797313): adapt this when the output implementation is set.
    // Selected items items count should show, so 1 less.
    assertTrue(firstSectionTitle.textContent!.trim().includes(
        '(' + sectionItemCount + ')'));
  });

  test('ExpandingSections', async function() {
    assertTrue(isVisible(batchUploadApp));

    const expandButtons =
        batchUploadApp.shadowRoot!.querySelectorAll('.expand-button');
    const collapseSections =
        batchUploadApp.shadowRoot!.querySelectorAll('.data-items-collapse');
    // TODO(b/363204445): check based on the test data.
    assertEquals(expandButtons.length, 2);
    assertEquals(collapseSections.length, 2);

    const firstCollapseSection = collapseSections[0] as CrCollapseElement;
    const firstExpandButton = expandButtons[0]! as CrExpandButtonElement;
    // Section is collapsed by default.
    assertFalse(firstCollapseSection.opened);

    // Expanding the section on click.
    firstExpandButton.click();
    await microtasksFinished();
    assertTrue(firstCollapseSection.opened);

    // Collapsing the section second click.
    firstExpandButton.click();
    await microtasksFinished();
    assertFalse(firstCollapseSection.opened);
  });

  // TODO(b/359797313): Add more tests related to clicking save and the expected
  // output based on the selected items.
});
