// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('ContentProcessing.sanitizeLinks', function() {
  let testContainer;

  setup(function() {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(function() {
    document.body.removeChild(testContainer);
    testContainer = null;
  });

  test(
      'sanitizeLinks should keep valid http/https links and open in new tab',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="http://example.com">HTTP Link</a>' +
            '<a href="https://example.com">HTTPS Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 2);
        assert.equal(links[0].getAttribute('href'), 'http://example.com');
        assert.equal(links[0].target, '_blank');
        assert.equal(links[1].getAttribute('href'), 'https://example.com');
        assert.equal(links[1].target, '_blank');
      });

  test('sanitizeLinks should remove javascript links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<a href="javascript:void(0)">JS Link</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'JS Link');
  });

  test(
      'sanitizeLinks should keep #in-page links and not open in new tab',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="#anchor">Anchor Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 1);
        assert.equal(links[0].getAttribute('href'), '#anchor');
        // Should NOT be _blank.
        assert.notEqual(links[0].target, '_blank');
      });

  test(
      'sanitizeLinks should keep #in-page links with path-like characters',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="#foo/bar">Hash Path Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 1);
        assert.equal(links[0].getAttribute('href'), '#foo/bar');
        assert.notEqual(links[0].target, '_blank');
      });

  test('sanitizeLinks should remove empty hash links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<a href="#">Empty Hash</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'Empty Hash');
  });

  test('sanitizeLinks should remove mailto links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML =
        '<a href="mailto:user@example.com">Mailto Link</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'Mailto Link');
  });
});

suite('ContentProcessing.removeExtraneousElementsFrom', function() {
  let testContainer;

  setup(function() {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(function() {
    document.body.removeChild(testContainer);
    testContainer = null;
  });

  test('removes UI labels', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<div>Advertisement</div>' +
        '<p>Sponsored</p>' +
        '<span>Supported by</span>' +
        '<article>Skip Advertisement</article>' +
        '<div>Legitimate content with advertisement inside a long paragraph ' +
        'that exceeds the forty character limit by quite a bit.</div>';

    removeExtraneousElementsFrom(testContainer);

    const remainingDivs = testContainer.querySelectorAll('div');
    assert.equal(remainingDivs.length, 1);
    assert.isTrue(remainingDivs[0].textContent.includes('Legitimate content'));
    assert.equal(testContainer.querySelectorAll('p').length, 0);
    assert.equal(testContainer.querySelectorAll('span').length, 0);
    assert.equal(testContainer.querySelectorAll('article').length, 0);
  });

  test('removes accessibility announcements', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<div aria-live="polite">Loading...</div>' +
        '<div role="alert">Error</div>' +
        '<div role="status">Saved</div>' +
        '<div role="log">Log entry</div>' +
        '<div aria-live="polite">This is a very long live blog entry ' +
        'that should not be removed because it is well over the two ' +
        'hundred character limit. It contains actual content that users ' +
        'will want to read during a live event. We are adding more text ' +
        'to ensure it surpasses the length threshold reliably.</div>';

    removeExtraneousElementsFrom(testContainer);

    const remainingDivs = testContainer.querySelectorAll('div');
    assert.equal(remainingDivs.length, 1);
    assert.isTrue(remainingDivs[0].textContent.includes('very long live blog'));
  });

  test('removes visually hidden empty elements', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<div style="width: 0; height: 0;"></div>' +
        '<span style="width: 0; height: 0;">  </span>' +
        '<div style="width: 0; height: 0;">Text</div>' +
        '<div style="width: 10px; height: 0;"></div>' +
        '<img style="width: 0; height: 0;">' +
        '<div style="float: left; height: 0;"></div>';

    // Simulate getBoundingClientRect for testing
    const elements = testContainer.querySelectorAll('*');
    for (const el of elements) {
      el.getBoundingClientRect = function() {
        const style = this.getAttribute('style') || '';
        return {
          width: style.includes('width: 0') ? 0 : 10,
          height: style.includes('height: 0') ? 0 : 10,
        };
      };
    }

    removeExtraneousElementsFrom(testContainer);

    // The first two divs/spans are 0x0 and empty, so they are removed.
    // The third has text, so it stays.
    // The fourth has width 10, so it stays.
    // The fifth is an img (not in allowlist), so it stays.
    // The sixth has width 10 (simulated float), so it stays.
    const remainingElements = testContainer.querySelectorAll('*');
    assert.equal(remainingElements.length, 4);
    assert.equal(remainingElements[0].tagName, 'DIV');
    assert.equal(remainingElements[0].textContent, 'Text');
    assert.equal(remainingElements[1].tagName, 'DIV');
    assert.equal(remainingElements[2].tagName, 'IMG');
    assert.equal(remainingElements[3].tagName, 'DIV');
  });

  test('removes nested extraneous elements correctly', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML =
        '<div id="keep1">This is a very long paragraph that will definitely ' +
        'exceed the one hundred character limit to ensure parent is kept. ' +
        'Keep Me' +
        '  <div id="remove1">Advertisement' +
        '    <div id="child1">Nested Child</div>' +
        '  </div>' +
        '</div>' +
        '<div id="keep2">Keep Me Too</div>';

    removeExtraneousElementsFrom(testContainer);

    // #remove1 and its child #child1 should be gone.
    assert.isNull(document.getElementById('remove1'));
    assert.isNull(document.getElementById('child1'));
    // #keep1 and #keep2 should remain.
    assert.isNotNull(document.getElementById('keep1'));
    assert.isNotNull(document.getElementById('keep2'));
    assert.isTrue(testContainer.textContent.includes('Keep Me'));
    assert.isTrue(testContainer.textContent.includes('Keep Me Too'));
    assert.isFalse(testContainer.textContent.includes('Advertisement'));
    assert.isFalse(testContainer.textContent.includes('Nested Child'));
  });

  test('removes the first child element correctly', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<div id="remove1">Advertisement</div>' +
        '<div id="keep1">Keep Me</div>';

    removeExtraneousElementsFrom(testContainer);

    assert.isNull(document.getElementById('remove1'));
    assert.isNotNull(document.getElementById('keep1'));
    assert.equal(testContainer.textContent.trim(), 'Keep Me');
  });
});
