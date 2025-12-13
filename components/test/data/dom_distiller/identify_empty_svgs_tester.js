// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('IdentifyEmptySVGsTest', function() {
  test('Correctly identifies SVG with href', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
    use.setAttribute('href', '#local-resource');
    svg.appendChild(use);
    container.appendChild(svg);
    document.body.appendChild(container);

    identifyEmptySVGs(container);

    assert.isTrue(svg.classList.contains('distilled-svg-with-local-ref'));

    document.body.removeChild(container);
  });

  test('Correctly identifies SVG with xlink:href', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
    use.setAttributeNS('http://www.w3.org/1999/xlink', 'href', '#local-resource');
    svg.appendChild(use);
    container.appendChild(svg);
    document.body.appendChild(container);

    identifyEmptySVGs(container);

    assert.isTrue(svg.classList.contains('distilled-svg-with-local-ref'));

    document.body.removeChild(container);
  });

  test('Ignores SVG without local resource pointer', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
    use.setAttribute('href', 'http://example.com#resource');
    svg.appendChild(use);
    container.appendChild(svg);
    document.body.appendChild(container);

    identifyEmptySVGs(container);

    assert.isFalse(svg.classList.contains('distilled-svg-with-local-ref'));

    document.body.removeChild(container);
  });

  test('Ignores SVG without use element', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    container.appendChild(svg);
    document.body.appendChild(container);

    identifyEmptySVGs(container);

    assert.isFalse(svg.classList.contains('distilled-svg-with-local-ref'));

    document.body.removeChild(container);
  });
});
