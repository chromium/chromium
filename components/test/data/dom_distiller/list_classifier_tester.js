// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ListClassifier class in
 * dom_distiller_viewer.js.
 */

suite('ListClassifier', function() {
  let testContainer;

  setup(function() {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(function() {
    document.body.removeChild(testContainer);
  });

  const runTest = async (html, expected) => {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = html;
    ListClassifier.processListsIn(testContainer);
    const el = testContainer.firstElementChild;
    const hasClass = el.classList.contains('distilled-content-list');
    assert.equal(hasClass, expected);
  };

  test('should reject empty lists', async function() {
    await runTest('<ul></ul>', false);
    await runTest('<ol></ol>', false);
  });

  test('should reject navigational lists', async function() {
    const htmlByClass = '<ul class="menu"><li><a href="#">Item</a></li></ul>';
    await runTest(htmlByClass, false);
    const htmlById = '<ul id="sidebar-nav"><li><a href="#">Item</a></li></ul>';
    await runTest(htmlById, false);
  });

  test('should accept ordered lists', async function() {
    const html =
        '<ol><li>Vanadium</li><li>Chromium</li><li>Manganese</li></ol>';
    await runTest(html, true);
  });

  test('should accept lists with high punctuation ratio', async function() {
    const html = '<ul><li>Invent.</li><li>Test?</li><li>Profit!</li></ul>';
    await runTest(html, true);
  });

  test(
      'should accept link-dominant lists with long link text',
      async function() {
        const html = `<ul>
            <li><a href="#">To be, or not to be, that is the question</a></li>
            <li><a href="#">Endless forms most beautiful</a></li>
          </ul>`;
        await runTest(html, true);
      });

  test(
      'should reject link-dominant lists with short link text',
      async function() {
        const html = `<ul>
            <li><a href="#">Strange</a></li>
            <li><a href="#">Charm</a></li>
          </ul>`;
        await runTest(html, false);
      });

  test('should accept text-dominant lists', async function() {
    const html = `<ul>
        <li>Flavor text that comes with <a href="#">a link</a>.</li>
      </ul>`;
    await runTest(html, true);
  });

  test(
      'should reject lists with substantive non-link elements',
      async function() {
        const html = `<ul>
            <li><a href="#">Item 1</a><img src="image1.jpg"/></li>
            <li><a href="#">Item 2</a></li>
          </ul>`;
        await runTest(html, false);
      });

  test('should accept link lists with simple wrappers', async function() {
    const html = `<ul>
          <li><span><a href="#">A link inside a span element</a></span></li>
        </ul>`;
    await runTest(html, true);
  });
});
