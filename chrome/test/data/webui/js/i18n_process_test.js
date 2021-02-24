// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for ui/webui/resources/js/i18n_template_no_process.js

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
    '//ui/webui/resources/js/i18n_template_no_process.js',
    '//ui/webui/resources/js/load_time_data.js',
    '//third_party/mocha/mocha.js',
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

    test('attributes', function() {
      assertNotEquals('', document.title);
      assertTrue(document.body.hasAttribute('type'));
      assertTrue(document.querySelector('span').textContent.length > 5);
      assertEquals('none', document.querySelector('span').style.display);
    });

    test('fragment', function() {
      var span = document.createElement('span');
      span.setAttribute('i18n-content', 'content');

      var docFrag = document.createDocumentFragment();
      docFrag.appendChild(span);

      var div = document.createElement('div');
      docFrag.appendChild(div);

      i18nTemplate.process(docFrag, loadTimeData);

      assertTrue(span.hasAttribute('i18n-processed'));
      assertNotEquals('', span.textContent);

      assertTrue(div.hasAttribute('i18n-processed'));
    });

    test('rerun', function() {
      document.body.removeAttribute('type');
      i18nTemplate.process(document, loadTimeData);
      assertTrue(document.body.hasAttribute('type'));
    });

    test('templates', function() {
      var outerDocFrag = document.querySelector('template').content;
      var innerDocFrag = outerDocFrag.querySelector('template').content;
      assertNotEquals('', innerDocFrag.querySelector('div').textContent);
    });
  });
  mocha.run();
});
