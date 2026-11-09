// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A utility class for classifying images in distilled content.
 *
 * Uses a prioritized cascade of heuristics to classify an image as either
 * inline (e.g., icon) or full-width (e.g., feature image). The checks are:
 * 1. Rendered size vs. viewport size (for visually dominant images).
 * 2. Intrinsic size and metadata (for small or decorative images).
 * 3. Structural context (e.g., inside a <figure>).
 * 4. A final fallback based on intrinsic width.
 *
 * All checks use density-independent units (CSS pixels).
 */
class ImageClassifier {
  static INLINE_CLASS = 'distilled-inline-img';
  static FULL_WIDTH_CLASS = 'distilled-full-width-img';
  static DOMINANT_IMAGE_MIN_VIEWPORT_RATIO = 0.8;
  static SRC_CANDIDATES = [
    // Used by lazysizes and UI Frameworks like Bootstrap.
    'data-src',
    // Mostly used in websites that depend on jQuery LazyLoad.
    'data-original',
    // WordPress standard, injected by plugins like WP Rocket or Smush.
    'data-lazy-src',
    // Other implementations.
    'data-url',
    'data-image',
  ];
  static SRCSET_CANDIDATES = [
    'data-srcset',
    'data-lazy-srcset',
    'data-original-set',
  ];
  static SIZES_CANDIDATES = [
    'data-sizes',
    'data-lazy-sizes',
  ];
  static IMG_BLOCK_LEVEL_CONTAINER_TAGS = ['P', 'DIV', 'FIGURE', 'BODY'];
  static NON_WHITESPACE_REGEXP = /\S/;

  constructor() {
    // Baseline thresholds in density-independent units (CSS pixels).
    this.smallAreaUpperBoundDp = 64 * 64;
    this.inlineWidthFallbackUpperBoundDp = 300;
    this.loneImageMinWidthDp = 150;

    // Matches common keywords for icons or mathematical formulas.
    const mathyKeywords =
        ['math', 'latex', 'equation', 'formula', 'tex', 'icon'];
    this._mathyKeywordsRegex =
        new RegExp('\\b(' + mathyKeywords.join('|') + ')\\b', 'i');

    // Matches characters commonly found in inline formulas.
    this._mathyAltTextRegex = /[+\-=_^{}\\]/;

    // Extracts the filename from a URL path.
    this._filenameRegex = /(?:.*\/)?([^?#]*)/;

    // Cache for _getContainerStats() to avoid re-computing for the same
    // container.
    this._containerStatsCache = new Map();
  }

  /**
   * Checks quickly whether an image is visually dominant.
   * @param {HTMLImageElement} img The image element to check.
   * @return {boolean} Whether the image is visually dominant.
   * @private
   */
  _isImageVisuallyDominant(img) {
    const renderedWidth = img.getBoundingClientRect().width;
    if (renderedWidth > 0 && window.innerWidth > 0 &&
        (renderedWidth / window.innerWidth) >
            ImageClassifier.DOMINANT_IMAGE_MIN_VIEWPORT_RATIO) {
      return ImageClassifier.FULL_WIDTH_CLASS;
    }
  }

  /**
   * Checks for strong signals that the image is INLINE based on its intrinsic
   * properties.
   * @param {HTMLImageElement} img The image element to check.
   * @return {boolean} True if the image should be inline.
   * @private
   */
  _isDefinitelyInline(img) {
    // Use natural dimensions (in CSS pixels) to check for small area.
    const area = img.naturalWidth * img.naturalHeight;
    if (area > 0 && area < this.smallAreaUpperBoundDp) {
      return true;
    }

    // "Mathy" or decorative clues in attributes.
    const classAndId = (img.className + ' ' + img.id);
    if (this._mathyKeywordsRegex.test(classAndId)) {
      return true;
    }

    // Check the filename of the src URL, ignoring data URIs.
    if (img.src && !img.src.startsWith('data:')) {
      const filename = img.src.match(this._filenameRegex)?.[1] || '';
      if (filename && this._mathyKeywordsRegex.test(filename)) {
        return true;
      }
    }

    // "Mathy" alt text.
    const alt = img.getAttribute('alt') || '';
    if (alt.length > 0 && alt.length < 80 &&
        this._mathyAltTextRegex.test(alt)) {
      return true;
    }

    return false;
  }

  /**
   * Computes various stats on a container and unconditionally writes to cache.
   * @param {Element} container The containing elements.
   * @return {{imgCount: number, hasText: boolean}} Container stats.
   * @private
   */
  _addContainerStats(container) {
    const stats = {
      imgCount: container.querySelectorAll('img').length,
      hasText:
          ImageClassifier.NON_WHITESPACE_REGEXP.test(container.textContent),
    };
    this._containerStatsCache.set(container, stats);
    return stats;
  }

  /**
   * Gets stats (image count, text presence) for a container, with caching.
   * @param {Element} container The container element.
   * @return {{imgCount: number, hasText: boolean}} Container stats.
   * @private
   */
  _getContainerStats(container) {
    return this._containerStatsCache.get(container) ??
        this._addContainerStats(container);
  }

  /**
   * Checks if the given image is the only significant content within its
   * nearest block-level container.
   * @param {HTMLImageElement} img The image element to check.
   * @return {boolean} Whether the image is the lone significant content.
   * @private
   */
  _isLoneImageInContainer(img) {
    let container = img.parentElement;
    // Find `img`'s nearest relevant block-level container.
    while (container &&
           !ImageClassifier.IMG_BLOCK_LEVEL_CONTAINER_TAGS.includes(
               container.tagName)) {
      container = container.parentElement;
    }
    if (!container) {
      return false;
    }

    const {imgCount, hasText} = this._getContainerStats(container);
    // `img` should be alone, and all text should be whitespace.
    return imgCount === 1 && !hasText;
  }

  /**
   * Checks if the image is the primary content of its container.
   * @param {HTMLImageElement} img The image element to check.
   * @return {boolean} True if the image should be full-width.
   * @private
   */
  _isDefinitelyFullWidth(img) {
    const parent = img.parentElement;

    // Image is in a <figure> with a <figcaption>.
    if (parent && parent.tagName === 'FIGURE' &&
        parent.querySelector('figcaption')) {
      return true;
    }

    // Image is a medium-to-large standalone image.
    if (img.naturalWidth > this.loneImageMinWidthDp &&
        this._isLoneImageInContainer(img)) {
      return true;
    }

    return false;
  }

  /**
   * Classifies the image based on a simple intrinsic width fallback.
   * @param {HTMLImageElement} img The image element to check.
   * @return {string} The CSS class to apply.
   * @private
   */
  _classifyByFallback(img) {
    // Use naturalWidth (in CSS pixels) and compare against the dp threshold.
    return img.naturalWidth > this.inlineWidthFallbackUpperBoundDp ?
        ImageClassifier.FULL_WIDTH_CLASS :
        ImageClassifier.INLINE_CLASS;
  }

  /**
   * Detects lazy-loading attributes and moves them to standard attributes
   * (src, srcset, sizes) to trigger native loading.
   * This is necessary for static environments where the original page's
   * lazy-loading JavaScript does not run, ensuring the real content is loaded
   * instead of a placeholder.
   * @param {HTMLImageElement} img The image element to check.
   * @private
   */
  _loadLazyImageAttributes(img) {
    if (!img.src || img.src.startsWith('data:')) {
      const srcAttribute =
          ImageClassifier.SRC_CANDIDATES.find(el => img.hasAttribute(el));
      if (srcAttribute) {
        img.src = img.getAttribute(srcAttribute);
        img.removeAttribute(srcAttribute);
      }
    }

    const srcsetAttribute =
        ImageClassifier.SRCSET_CANDIDATES.find(el => img.hasAttribute(el));
    if (srcsetAttribute) {
      img.srcset = img.getAttribute(srcsetAttribute);
      img.removeAttribute(srcsetAttribute);
    }

    const sizeAttribute =
        ImageClassifier.SIZES_CANDIDATES.find(el => img.hasAttribute(el));
    if (sizeAttribute) {
      img.sizes = img.getAttribute(sizeAttribute);
      img.removeAttribute(sizeAttribute);
    }
  }

  /**
   * Determines an image's display style using a prioritized cascade of checks.
   * @param {HTMLImageElement} img The image element to classify.
   * @return {string} The CSS class to apply.
   */
  classify(img) {
    // Check for visually dominant images first, as this is the most reliable
    // signal and overrides all other heuristics.
    if (this._isImageVisuallyDominant(img)) {
      return ImageClassifier.FULL_WIDTH_CLASS;
    }

    // Fall back to checks based on intrinsic properties and structure.
    if (this._isDefinitelyInline(img)) {
      return ImageClassifier.INLINE_CLASS;
    }

    if (this._isDefinitelyFullWidth(img)) {
      return ImageClassifier.FULL_WIDTH_CLASS;
    }

    return this._classifyByFallback(img);
  }

  /**
   * Applies classification to all images within an element.
   * @param {HTMLElement} element The element to search for images in.
   */
  static processImagesIn(element) {
    const classifier = new ImageClassifier();
    const images = element.getElementsByTagName('img');

    const imageLoadHandler = (event) => {
      const img = event.currentTarget;
      const classification = classifier.classify(img);
      img.classList.add(classification);
    };

    for (const img of images) {
      classifier._loadLazyImageAttributes(img);
      img.onload = imageLoadHandler;

      // If the image is already loaded (e.g., from cache), manually trigger.
      if (img.complete) {
        // We use .call() to ensure `this` is correctly bound if the handler
        // were a traditional function, and to pass a mock event object.
        imageLoadHandler.call(img, {currentTarget: img});
      }
    }
  }
}
