// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SVG_NS = 'http://www.w3.org/2000/svg';

/**
 * @fileoverview Test suite for the ImageClassifier class in
 * image_classifier.js.
 */

suite('ImageClassifier', function() {
  let testContainer;

  /**
   * Helper to create an image with specific natural dimensions, append it to
   * the container, and return a promise that resolves when the image is ready
   * for classification.
   * @param {string} id A unique ID for the image.
   * @param {number} naturalWidth The desired natural width of the image in CSS
   *     pixels.
   * @param {number} naturalHeight The desired natural height of the image in
   *     CSS pixels.
   * @param {string} parentTag The tag of the parent element (e.g., 'p', 'div').
   * @param {string} parentContent The innerHTML of the parent element, with
   *     '<img>' as a placeholder for the image.
   * @param {Object} attributes An object of attributes to set on the image.
   * @return {Promise<HTMLImageElement>}
   */
  function createImageTest(
      id, naturalWidth, naturalHeight, parentTag, parentContent,
      attributes = {}) {
    return new Promise((resolve) => {
      const parent = document.createElement(parentTag);
      // Create a placeholder for replacement.
      parent.innerHTML = parentContent.replace('<img>', `<img id="${id}">`);
      testContainer.appendChild(parent);

      const img = document.getElementById(id);

      // The promise resolves when the image is loaded or has failed to load.
      img.onload = () => resolve(img);
      img.onerror = () => resolve(img);

      // Apply all attributes *except* `src` to avoid triggering a network
      // request before the load/error handlers are attached.
      const imgSrc = attributes.src;
      for (const [key, value] of Object.entries(attributes)) {
        if (key !== 'src') {
          img.setAttribute(key, value);
        }
      }

      // Set `src` last to ensure handlers are ready.
      if (imgSrc) {
        img.src = imgSrc;
      } else {
        // Generate an SVG `src` to control dimensions if one wasn't provided.
        const svg = `<svg width="${naturalWidth}" height="${naturalHeight}"` +
            ` xmlns="${SVG_NS}"></svg>`;
        img.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
      }

      // For synchronously loaded images (e.g., data URIs), resolve immediately
      // if complete to prevent missed load events and test timeouts.
      if (img.complete) {
        resolve(img);
      }
    });
  }

  /**
   * Ensures that all images added using createImageTest() have been loaded into
   * DOM, so that attempts to get image dimensions using `naturalWidth` /
   * `naturalHeight` attributes or getBoundingClientRect() would produce the
   * intended value.
   * @param {Array<Promise<HTMLImageElement>>}
   * @return {Promise}
   */
  async function ensureTestContainerImageLoad(imagePromises) {
    const {assert} = await import('./index.js');
    // Wait for all images to be created and loaded into the DOM.
    await Promise.all(imagePromises);

    // Yield to the browser's event loop with a `setTimeout` to ensure a layout
    // pass occurs before the classifier runs. This is critical for
    // getBoundingClientRect() to return a non-zero width.
    await new Promise(resolve => setTimeout(resolve, 0));

    for (const img of testContainer.querySelectorAll('img')) {
      // Handle exceptions for test images with fake src.
      if (img.getAttribute('src').startsWith('/')) {
        continue;
      }
      assert.isAbove(img.naturalWidth, 0, `Image #${img.id} should have width`);
      assert.isAbove(
          img.naturalHeight, 0, `Image #${img.id} should have height`);
    }
  }

  setup(function() {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(function() {
    document.body.removeChild(testContainer);
  });

  test('should classify images and apply correct classes', async function() {
    const {assert} = await import('./index.js');

    // Define all test dimensions in density-independent units (DP).
    const INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP = 300;

    // For metadata tests, use a width that is large but guaranteed to not
    // trigger the dominant image guardrail.
    const safeNonDominantWidth = Math.floor(window.innerWidth * 0.7);

    const imagePromises = [
      // New Guardrail Test: A visually dominant "hero" image.
      createImageTest('hero_image', 200, 100, 'p', '<img>', {
        style: 'width: 90vw;',
        // Add a misleading keyword to prove the guardrail overrides it.
        class: 'icon-class',
      }),

      // Definitely Inline (based on intrinsic properties).
      createImageTest('small_area', 50, 50, 'p', '<img>'),
      // These should be inline due to metadata, using a width that is
      // guaranteed to not be visually dominant.
      createImageTest(
          'math_class', safeNonDominantWidth, 100, 'p', '<img>',
          {class: 'tex'}),
      createImageTest(
          'math_filename', safeNonDominantWidth, 100, 'p', '<img>',
          {src: '/foo/icon.svg'}),
      createImageTest(
          'math_alt', safeNonDominantWidth, 100, 'p', '<img>', {alt: 'E=mc^2'}),

      // Definitely full-width, based on caption.
      createImageTest(
          'figure_with_caption', 400, 300, 'figure',
          '<img><figcaption>Test</figcaption>'),

      // Definitely full-width, based on lone image.
      // Various cases for "lone image" heuristics.
      // Width must be > 150 dp to trigger heuristics.
      createImageTest('lone_image_in_p', 180, 300, 'p', '<img>'),
      createImageTest('lone_image_with_space', 180, 300, 'p', ' <img> <b></b>'),
      createImageTest('lone_image_with_br', 180, 300, 'p', '<img><br>'),
      createImageTest(
          'lone_image_nested', 180, 300, 'div', '<span><img></span>'),

      // Lone image heuristic failures (letting fallback assign to inline).
      createImageTest('lone_image_in_p_small', 120, 100, 'p', '<img>'),
      createImageTest(
          'lone_image_nested_pair', 180, 300, 'div',
          '<span><img><img id="other" src="/foo/other.png"></img></span>'),

      // Fallback (based on intrinsic width).
      createImageTest(
          'fallback_wide', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP + 50, 200, 'p',
          'Some text <img>'),
      createImageTest(
          'fallback_narrow', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP - 50, 200,
          'p', 'Some text <img>'),
    ];
    await ensureTestContainerImageLoad(imagePromises);

    // Run the classifier on the container. For cached images, this will
    // synchronously apply the classification classes.
    ImageClassifier.processImagesIn(testContainer);

    // Run the assertions.
    const assertHasClass = (id, expectedClass) => {
      const el = document.getElementById(id);
      assert.isTrue(el.classList.contains(expectedClass),
          `Image #${id} should have class ${expectedClass}`);
    };

    assertHasClass('hero_image', ImageClassifier.FULL_WIDTH_CLASS);

    assertHasClass('small_area', ImageClassifier.INLINE_CLASS);
    assertHasClass('math_class', ImageClassifier.INLINE_CLASS);
    assertHasClass('math_filename', ImageClassifier.INLINE_CLASS);
    assertHasClass('math_alt', ImageClassifier.INLINE_CLASS);

    assertHasClass('figure_with_caption', ImageClassifier.FULL_WIDTH_CLASS);

    assertHasClass('lone_image_in_p', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('lone_image_with_space', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('lone_image_with_br', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('lone_image_nested', ImageClassifier.FULL_WIDTH_CLASS);

    assertHasClass('lone_image_in_p_small', ImageClassifier.INLINE_CLASS);
    assertHasClass('lone_image_nested_pair', ImageClassifier.INLINE_CLASS);

    assertHasClass('fallback_wide', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('fallback_narrow', ImageClassifier.INLINE_CLASS);
  });

  test('should cache container stats', async function() {
    const {assert} = await import('./index.js');

    const NUM_IMG = 100;
    const IMAGE_WIDTH = 200;  // > ImageClassifier.loneImageMinWidthDp.

    const imagePromises = [];
    for (let i = 0; i < NUM_IMG; i++) {
      imagePromises.push(createImageTest(
          `img-${i}`, IMAGE_WIDTH, 100, 'span',
          'Some text before <img> some text after<br/>'));
    }
    await ensureTestContainerImageLoad(imagePromises);

    const classifier = new ImageClassifier();

    // Spy on the expensive computation method.
    let callCount = 0;
    const originalAddStats = classifier._addContainerStats;
    classifier._addContainerStats = function(c) {
      callCount++;
      return originalAddStats.call(this, c);
    };

    // Call _isDefinitelyFullWidth() for every image. These images share the
    // same container, so _addContainerStats() gets called once due to caching.
    for (const img of testContainer.querySelectorAll('img')) {
      classifier._isDefinitelyFullWidth(img);
    }

    // Restore the original method.
    classifier._addContainerStats = originalAddStats;

    // The container for all `NUM_IMG` images is the `div` we created, which
    // should only have had its stats computed once.
    assert.strictEqual(
        callCount, 1, '_addContainerStats() should be called once only');
  });

  test('should detect and load lazy-loaded image attributes', async function() {
    return;
    const {assert} = await import('./index.js');

    const MODERN_URL = 'https://example.com/modern.jpg';
    const LEGACY_URL = 'https://example.com/legacy.jpg';
    const WP_URL = 'https://example.com/wordpress.jpg';
    const WP_SRCSET = [
      'https://example.com/wp-400.jpg 400w',
      'https://example.com/wp-800.jpg 800w',
    ].join(',');
    const WP_SIZES = '(max-width: 600px) 100vw, 600px';

    const imagePromises = [
      createImageTest(
          'lazy_modern', 100, 100, 'p', '<img>', {'data-src': MODERN_URL}),
      createImageTest(
          'lazy_legacy', 100, 100, 'p', '<img>', {'data-original': LEGACY_URL}),
      createImageTest('lazy_wordpress', 100, 100, 'p', '<img>', {
        'data-lazy-src': WP_URL,
        'data-lazy-srcset': WP_SRCSET,
        'data-lazy-sizes': WP_SIZES,
      }),
      createImageTest('lazy_priority', 100, 100, 'p', '<img>', {
        'data-src': MODERN_URL,
        'data-original': LEGACY_URL,
      }),
    ];
    await ensureTestContainerImageLoad(imagePromises);

    ImageClassifier.processImagesIn(testContainer);

    const imgModern = document.getElementById('lazy_modern');
    assert.equal(
        imgModern.getAttribute('src'), MODERN_URL,
        'Should promote data-src to src');
    assert.isFalse(
        imgModern.hasAttribute('data-src'),
        'Should remove data-src after promotion');

    const imgLegacy = document.getElementById('lazy_legacy');
    assert.equal(
        imgLegacy.getAttribute('src'), LEGACY_URL,
        'Should promote data-original to src');

    const imgWP = document.getElementById('lazy_wordpress');
    assert.equal(
        imgWP.getAttribute('src'), WP_URL,
        'Should promote data-lazy-src to src');
    assert.equal(
        imgWP.getAttribute('srcset'), WP_SRCSET,
        'Should promote data-lazy-srcset to srcset');
    assert.equal(
        imgWP.getAttribute('sizes'), WP_SIZES,
        'Should promote data-lazy-sizes to sizes');

    const imgPriority = document.getElementById('lazy_priority');
    assert.equal(
        imgPriority.getAttribute('src'), MODERN_URL,
        'Should prioritize data-src over data-original');
  });
});
