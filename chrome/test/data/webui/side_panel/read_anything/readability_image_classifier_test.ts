// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadabilityImageClassifier} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('ReadabilityImageClassifier', function() {
  let testContainer: HTMLElement;

  function createImageTest(
      id: string, naturalWidth: number, naturalHeight: number,
      parentTag: string, parentContent: string,
      attributes: Record<string, string> = {}): Promise<HTMLImageElement> {
    return new Promise((resolve) => {
      const parent = document.createElement(parentTag);
      const img = document.createElement('img');
      img.id = id;

      if (parentContent.includes('Some text')) {
        parent.appendChild(document.createTextNode('Some text '));
      }
      parent.appendChild(img);

      if (parentContent.includes('figcaption')) {
        const caption = document.createElement('figcaption');
        caption.textContent = 'Test';
        parent.appendChild(caption);
      }

      if (parentContent.includes('<br>')) {
        parent.appendChild(document.createElement('br'));
      }

      testContainer.appendChild(parent);

      // The promise resolves when the image is loaded or has failed to load.
      img.onload = () => resolve(img);
      img.onerror = () => resolve(img);

      // Apply all attributes *except* src to avoid triggering a network
      // request before the load/error handlers are attached.
      const imgSrc = attributes['src'];
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
        const svg = `<svg width="${naturalWidth}" height="${
            naturalHeight}" xmlns="http://www.w3.org/2000/svg"></svg>`;
        img.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
      }
    });
  }

  setup(() => {
    testContainer = document.createElement('div');
    testContainer.style.width = '100%';
    document.body.appendChild(testContainer);
  });

  teardown(() => {
    document.body.removeChild(testContainer);
  });

  test('should classify images and apply correct classes', async () => {
    const inlineWidthFallbackUpperBoundDp = 300;
    const safeNonDominantWidth = Math.floor(window.innerWidth * 0.7);

    const imagePromises: Array<Promise<HTMLImageElement>> = [
      // 1. Hero Image
      createImageTest('hero_image', 200, 100, 'p', '<img>', {
        style: 'width: 90vw;',
        class: 'hero-style',
      }),

      // 2. Definitely Inline (small area)
      createImageTest('small_area', 50, 50, 'p', '<img>'),

      // 3. Inline via Metadata
      createImageTest(
          'math_class', safeNonDominantWidth, 100, 'p', '<img>',
          {class: 'tex', src: 'url1.jpg'}),
      createImageTest(
          'math_filename', safeNonDominantWidth, 100, 'p', '<img>',
          {src: '/foo/icon.svg'}),
      createImageTest(
          'math_alt', safeNonDominantWidth, 100, 'p', '<img>',
          {alt: 'E=mc^2', src: 'url2.jpg'}),

      // 4. Definitely Full-width (Structure)
      createImageTest(
          'figure_with_caption', 400, 300, 'figure',
          '<img><figcaption>Test</figcaption>'),
      createImageTest(
          'sole_content_in_p', 400, 300, 'p', '<img>', {src: 'url3.jpg'}),
      createImageTest(
          'sole_content_with_br', 400, 300, 'p', '<img><br>',
          {src: 'url4.jpg'}),

      // 5. Fallback
      createImageTest(
          'fallback_wide', inlineWidthFallbackUpperBoundDp + 50, 200, 'p',
          'Some text <img>'),
      createImageTest(
          'fallback_narrow', inlineWidthFallbackUpperBoundDp - 50, 200, 'p',
          'Some text <img>'),
    ];

    await Promise.all(imagePromises);
    // Wait for layout to be ready.
    await new Promise(resolve => requestAnimationFrame(() => resolve(null)));

    ReadabilityImageClassifier.processImagesIn(testContainer);

    const assertHasClass = (id: string, expectedClass: string) => {
      const el = document.getElementById(id);
      assertTrue(!!el, `Image #${id} should exist`);
      assertTrue(
          el.classList.contains(expectedClass),
          `Image #${id} should have class ${expectedClass}. Classes: ${
              el.className}`);
    };

    const assertNotHasClass = (id: string, unexpectedClass: string) => {
      const el = document.getElementById(id);
      assertTrue(!!el, `Image #${id} should exist`);
      assertFalse(
          el.classList.contains(unexpectedClass),
          `Image #${id} should NOT have class ${unexpectedClass}`);
    };

    const INLINE = ReadabilityImageClassifier.INLINE_CLASS;
    const FULL = ReadabilityImageClassifier.FULL_WIDTH_CLASS;

    assertHasClass('hero_image', FULL);
    assertNotHasClass('hero_image', INLINE);
    assertHasClass('small_area', INLINE);
    assertHasClass('math_class', INLINE);
    assertHasClass('math_filename', INLINE);
    assertHasClass('math_alt', INLINE);

    assertHasClass('figure_with_caption', FULL);
    assertHasClass('sole_content_in_p', FULL);
    assertHasClass('sole_content_with_br', FULL);

    assertHasClass('fallback_wide', FULL);
    assertHasClass('fallback_narrow', INLINE);
  });

  test('should detect and load lazy-loaded image attributes', async () => {
    const modernURL = 'https://example.com/modern.jpg';
    const legacyURL = 'https://example.com/legacy.jpg';
    const modernURL2 = 'https://example.com/modern2.jpg';
    const legacyURL2 = 'https://example.com/legacy2.jpg';
    const wpURL = 'https://example.com/wordpress.jpg';
    const wpSrcSet = [
      'https://example.com/wp-400.jpg 400w',
      'https://example.com/wp-800.jpg 800w',
    ].join(',');
    const wpSizes = '(max-width: 600px) 100vw, 600px';
    const imagePromises = [
      createImageTest(
          'lazy_modern', 100, 100, 'p', '<img>', {'data-src': modernURL}),
      createImageTest(
          'lazy_legacy', 100, 100, 'p', '<img>', {'data-original': legacyURL}),
      createImageTest('lazy_wordpress', 100, 100, 'p', '<img>', {
        'data-lazy-src': wpURL,
        'data-lazy-srcset': wpSrcSet,
        'data-lazy-sizes': wpSizes,
      }),
      createImageTest('lazy_priority', 100, 100, 'p', '<img>', {
        'data-src': modernURL2,
        'data-original': legacyURL2,
      }),
    ];

    await Promise.all(imagePromises);
    ReadabilityImageClassifier.processImagesIn(testContainer);

    const imgModern = document.getElementById('lazy_modern');
    assertTrue(!!imgModern);
    assertEquals(
        imgModern.getAttribute('src'), modernURL,
        'Should promote data-src to src');
    assertFalse(
        imgModern.hasAttribute('data-src'),
        'Should remove data-src after promotion');

    const imgLegacy = document.getElementById('lazy_legacy');
    assertTrue(!!imgLegacy);
    assertEquals(
        imgLegacy.getAttribute('src'), legacyURL,
        'Should promote data-original to src');

    const imgWP = document.getElementById('lazy_wordpress');
    assertTrue(!!imgWP);
    assertEquals(
        imgWP.getAttribute('src'), wpURL,
        'Should promote data-lazy-src to src');
    assertEquals(
        imgWP.getAttribute('srcset'), wpSrcSet,
        'Should promote data-lazy-srcset to srcset');
    assertEquals(
        imgWP.getAttribute('sizes'), wpSizes,
        'Should promote data-lazy-sizes to sizes');

    const imgPriority = document.getElementById('lazy_priority');
    assertTrue(!!imgPriority);
    assertEquals(
        imgPriority.getAttribute('src'), modernURL2,
        'Should prioritize data-src over data-original');
  });

  test('should deduplicate nearby identical images', async () => {
    const src = 'https://example.com/dup.jpg';
    const alt = 'Duplicate Alt';

    const imagePromises = [
      createImageTest('img0', 100, 100, 'p', '<img>', {src, alt}),
      createImageTest('img1', 100, 100, 'p', '<img>', {src, alt}),
      createImageTest('img2', 100, 100, 'p', '<img>', {src: 'other.jpg'}),
      createImageTest('img3', 100, 100, 'p', '<img>', {src: 'other2.jpg'}),
      createImageTest('img4', 100, 100, 'p', '<img>', {src: 'other3.jpg'}),
      createImageTest('img5', 100, 100, 'p', '<img>', {src, alt}),
    ];

    await Promise.all(imagePromises);
    ReadabilityImageClassifier.processImagesIn(testContainer);

    assertTrue(!!document.getElementById('img0'), 'img0 should be kept');
    assertFalse(!!document.getElementById('img1'), 'img1 should be removed');
    assertTrue(!!document.getElementById('img2'), 'img2 should be kept');
    assertTrue(!!document.getElementById('img3'), 'img3 should be kept');
    assertTrue(!!document.getElementById('img4'), 'img4 should be kept');
    assertTrue(!!document.getElementById('img5'), 'img5 should be kept');
  });

  test('should remove parent FIGURE when deduplicating', async () => {
    const src = 'https://example.com/dup_fig.jpg';
    const alt = 'Figure Alt';

    const imagePromises = [
      createImageTest(
          'img_fig0', 100, 100, 'figure',
          '<img><figcaption>Caption</figcaption>', {src, alt}),
      createImageTest(
          'img_fig1', 100, 100, 'figure',
          '<img><figcaption>Caption</figcaption>', {src, alt}),
    ];

    await Promise.all(imagePromises);

    const fig0 = document.getElementById('img_fig0')?.parentElement;
    const fig1 = document.getElementById('img_fig1')?.parentElement;

    assertTrue(!!fig0 && fig0.tagName === 'FIGURE');
    assertTrue(!!fig1 && fig1.tagName === 'FIGURE');

    ReadabilityImageClassifier.processImagesIn(testContainer);

    assertTrue(testContainer.contains(fig0), 'fig0 should be kept');
    assertFalse(testContainer.contains(fig1), 'fig1 should be removed');
  });

  test(
      'should detect possibly transparent images and apply class', async () => {
        const TRANSPARENT_CLASS =
            ReadabilityImageClassifier.POSSIBLY_TRANSPARENT_CLASS;
        const imagePromises = [
          // 1. Standard transparent formats
          createImageTest(
              'img_png', 50, 50, 'p', '<img>',
              {src: 'https://example.com/logo.png'}),
          createImageTest('img_gif', 50, 50, 'p', '<img>', {src: 'anim.gif'}),
          createImageTest('img_avif', 50, 50, 'p', '<img>', {src: 'pic.avif'}),

          // 2. URLs with query parameters and hashes
          createImageTest(
              'img_svg_query', 50, 50, 'p', '<img>',
              {src: 'https://example.com/icon.svg?v=123'}),
          createImageTest(
              'img_webp_hash', 50, 50, 'p', '<img>',
              {src: '/images/asset.webp#anchor'}),

          // 3. Opaque formats, should not get the class
          createImageTest('img_jpg', 50, 50, 'p', '<img>', {src: 'photo.jpg'}),
          createImageTest(
              'img_jpeg', 50, 50, 'p', '<img>', {src: 'photo.jpeg?size=large'}),

          // 4. Data URIs (Base64)
          createImageTest(
              'data_png', 50, 50, 'p', '<img>',
              {src: 'data:image/png;base64,iVBORw0KGgo...'}),
          createImageTest(
              'data_svg', 50, 50, 'p', '<img>',
              {src: 'data:image/svg+xml;charset=utf-8,...'}),
          createImageTest(
              'data_jpg', 50, 50, 'p', '<img>',
            { src: 'data:image/jpeg;base64,/9j/4AAQSkZJRg...' }),
        ];

        await Promise.all(imagePromises);
        ReadabilityImageClassifier.processImagesIn(testContainer);

        const assertHasTransparentClass = (id: string, expected: boolean) => {
          const el = document.getElementById(id);
          assertTrue(!!el);
          assertEquals(expected, el.classList.contains(TRANSPARENT_CLASS));
        };

        // Assertions for transparent formats
        assertHasTransparentClass('img_png', true);
        assertHasTransparentClass('img_gif', true);
        assertHasTransparentClass('img_avif', true);
        assertHasTransparentClass('img_svg_query', true);
        assertHasTransparentClass('img_webp_hash', true);
        assertHasTransparentClass('data_png', true);
        assertHasTransparentClass('data_svg', true);

        // Assertions for opaque formats
        assertHasTransparentClass('img_jpg', false);
        assertHasTransparentClass('img_jpeg', false);
        assertHasTransparentClass('data_jpg', false);
      });
});
