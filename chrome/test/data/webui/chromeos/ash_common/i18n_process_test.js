// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for chrome/browser/resources/chromeos/arc_support/i18n_template_no_process.js

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */
function I18nProcessTest() {}

I18nProcessTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://dummyurl/',

  /**
   * The mocha adapter assumes all tests are async.
   * @override
   * @final
   */
  isAsync: true,

  /**
   * Files that need not be compiled.
   * @override
   */
  extraLibraries: [
    '//chrome/browser/resources/chromeos/arc_support/i18n_template_no_process.js',
    '//ui/webui/resources/js/load_time_data_deprecated.js',
    '//third_party/node/node_modules/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('I18nProcessTest', 'All', function() {
  suite('I18nProcessTest', function() {
    suiteSetup(function() {
      window.loadTimeData.data = {
        'content': 'doesn\'t matter; you can\'t see me!',
        'display': 'none',
        'title': 'BUY NOW!',
        'type': 'ectomorph',
        'nested': 'real deep',
      };
    });

    setup(function() {
      document.head.innerHTML = `
        <title i18n-content="title"></title>`;

      document.body.setAttribute('i18n-values', 'type:type');
      document.body.innerHTML = `
        <span i18n-values=".innerHTML:content;.style.display:display">
          &lt;3
        </span>
        <template>
          <template>
            <div i18n-content="nested"></div>
          </template>
        </template>`;

      i18nTemplate.process(document, loadTimeData);
    });

    test('attributes', async function() {
      const {assertNotEquals, assertTrue, assertEquals} =
          await import('chrome://webui-test/chai_assert.js');
      assertNotEquals('', document.title);
      assertTrue(document.body.hasAttribute('type'));
      assertTrue(document.querySelector('span').textContent.length > 5);
      assertEquals('none', document.querySelector('span').style.display);
    });

    test('fragment', async function() {
      const {assertNotEquals, assertTrue} =
          await import('chrome://webui-test/chai_assert.js');
      const span = document.createElement('span');
      span.setAttribute('i18n-content', 'content');

      const docFrag = document.createDocumentFragment();
      docFrag.appendChild(span);

      const div = document.createElement('div');
      docFrag.appendChild(div);

      i18nTemplate.process(docFrag, loadTimeData);

      assertTrue(span.hasAttribute('i18n-processed'));
      assertNotEquals('', span.textContent);

      assertTrue(div.hasAttribute('i18n-processed'));
    });

    test('rerun', async function() {
      const {assertTrue} = await import('chrome://webui-test/chai_assert.js');
      document.body.removeAttribute('type');
      i18nTemplate.process(document, loadTimeData);
      assertTrue(document.body.hasAttribute('type'));
    });

    test('templates', async function() {
      const {assertNotEquals} =
          await import('chrome://webui-test/chai_assert.js');
      const outerDocFrag = document.querySelector('template').content;
      const innerDocFrag = outerDocFrag.querySelector('template').content;
      assertNotEquals('', innerDocFrag.querySelector('div').textContent);
    });
  });
  mocha.run();
});
