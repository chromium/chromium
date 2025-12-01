// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ImageClassifier class in
 * dom_distiller_viewer.js.
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
        const svg = `<svg width="${naturalWidth}" height="${naturalHeight}"
                          xmlns="http://www.w3.org/2000/svg"></svg>`;
        img.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
      }
    });
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
      createImageTest(
          'hero_image', 200, 100, 'p', '<img>', {
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
          'math_alt', safeNonDominantWidth, 100, 'p', '<img>',
          {alt: 'E=mc^2'}),

      // Definitely Full-width (based on structure).
      createImageTest('figure_with_caption', 400, 300, 'figure',
                      '<img><figcaption>Test</figcaption>'),
      createImageTest('sole_content_in_p', 400, 300, 'p', '<img>'),
      createImageTest('sole_content_with_br', 400, 300, 'p', '<img><br>'),

      // Fallback (based on intrinsic width).
      createImageTest(
          'fallback_wide', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP + 50, 200, 'p',
          'Some text <img>'),
      createImageTest(
          'fallback_narrow', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP - 50, 200,
          'p', 'Some text <img>'),
    ];

    // Wait for all images to be created and loaded into the DOM.
    await Promise.all(imagePromises);

    // Yield to the browser's event loop with a `setTimeout` to ensure a layout
    // pass occurs before the classifier runs. This is critical for
    // getBoundingClientRect() to return a non-zero width.
    await new Promise(resolve => setTimeout(resolve, 0));

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
    assertHasClass('sole_content_in_p', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('sole_content_with_br', ImageClassifier.FULL_WIDTH_CLASS);

    assertHasClass('fallback_wide', ImageClassifier.FULL_WIDTH_CLASS);
    assertHasClass('fallback_narrow', ImageClassifier.INLINE_CLASS);
  });
});
