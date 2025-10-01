// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ImageClassifier class in
 * dom_distiller_viewer.js.
 */

suite('ImageClassifier', function() {
  let testContainer;

  // A solid red 1x1 pixel is used so that test images are visible, which
  // aids in visual debugging of the test harness.
  const RED_1X1_PIXEL = 'data:image/gif;base64,R0lGODlhAQABAIABAP8AAP///' +
                        'yH5BAEAAAEALAAAAAABAAEAAAICRAEAOw==';

  /**
   * Helper to create an image, append it to the container, and return a promise
   * that resolves when the image is ready for classification.
   * @param {string} id A unique ID for the image.
   * @param {number} widthDp The width of the image in DP.
   * @param {number} heightDp The height of the image in DP.
   * @param {string} parentTag The tag of the parent element (e.g., 'p', 'div').
   * @param {string} parentContent The innerHTML of the parent element, with
   *     '<img>' as a placeholder for the image.
   * @param {Object} attributes An object of attributes to set on the image.
   * @return {Promise<HTMLImageElement>}
   */
  function createImageTest(id, widthDp, heightDp, parentTag, parentContent,
                           attributes = {}) {
    return new Promise((resolve) => {
      const density = window.devicePixelRatio || 1;
      const parent = document.createElement(parentTag);
      const img = document.createElement('img');
      img.id = id;
      // Scale the density-independent dimensions to CSS pixels.
      img.width = widthDp * density;
      img.height = heightDp * density;

      for (const [key, value] of Object.entries(attributes)) {
        img.setAttribute(key, value);
      }

      // The promise resolves when the image is loaded or has failed to load.
      img.onload = () => resolve(img);
      img.onerror = () => resolve(img);

      parent.innerHTML = parentContent.replace('<img>', img.outerHTML);
      testContainer.appendChild(parent);

      // Set src last to ensure handlers are attached. Use the data URI only if
      // no src was provided in the test attributes.
      if (!img.hasAttribute('src')) {
        img.src = RED_1X1_PIXEL;
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
    const {assert} = await import('./chai.js');

    // Define all test dimensions in density-independent units (DP).
    const INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP = 300;

    const imagePromises = [
      // Definitely Inline.
      createImageTest('small_icon', 50, 50, 'p', '<img>'),
      createImageTest('math_class', 100, 100, 'p', '<img>', {class: 'tex'}),
      createImageTest('math_filename', 100, 100, 'p', '<img>',
                      {src: '/foo/icon.svg'}),
      createImageTest('math_alt', 100, 100, 'p', '<img>', {alt: 'E=mc^2'}),

      // Definitely Full-width.
      createImageTest('figure_with_caption', 400, 300, 'figure',
                      '<img><figcaption>Test</figcaption>'),
      createImageTest('sole_content_in_p', 400, 300, 'p', '<img>'),
      createImageTest('sole_content_with_br', 400, 300, 'p', '<img><br>'),

      // Fallback.
      createImageTest(
          'fallback_wide', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP + 50, 200, 'p',
          'Some text <img>'),
      createImageTest(
          'fallback_narrow', INLINE_WIDTH_FALLBACK_UPPER_BOUND_DP - 50, 200,
          'p', 'Some text <img>'),
    ];

    // Wait for all images to be created and loaded into the DOM.
    await Promise.all(imagePromises);

    // Run the classifier on the container. This may attach new onload handlers.
    ImageClassifier.processImagesIn(testContainer);

    // Wait one more event loop turn for the classifier's onload handlers to
    // fire.
    await new Promise(resolve => setTimeout(resolve, 0));

    // Run the assertions.
    const assertHasClass = (id, expectedClass) => {
      const el = document.getElementById(id);
      assert.isTrue(el.classList.contains(expectedClass),
          `Image #${id} should have class ${expectedClass}`);
    };

    assertHasClass('small_icon', ImageClassifier.INLINE_CLASS);
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
