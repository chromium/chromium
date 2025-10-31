// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(JSThemesAndFonts)

// These classes must agree with the font classes in distilledpage.css.
const themeClasses = ['light', 'dark', 'sepia'];
const fontFamilyClasses = ['sans-serif', 'serif', 'monospace'];

// LINT.ThenChange(//components/dom_distiller/core/viewer.cc:JSThemesAndFonts)

// On iOS, |distillerOnIos| was set to true before this script.
// eslint-disable-next-line no-var
var distillerOnIos;
if (typeof distillerOnIos === 'undefined') {
  distillerOnIos = false;
}

// The style guide recommends preferring $() to getElementById(). Chrome's
// standard implementation of $() is imported from chrome://resources, which the
// distilled page is prohibited from accessing. A version of it is
// re-implemented here to allow stylistic consistency with other JS code.
function $(id) {
  return document.getElementById(id);
}

/**
 * A helper function that calls the post-processing functions on a given
 * element.
 * @param {HTMLElement} element The container element of the article.
 */
function postProcessElement(element) {
  // Wrap tables to make them scrollable.
  wrapTables(element);

  // Readability will leave iframes around, but they need the proper structure
  // and classes to be styled correctly.
  addClassesToYoutubeIFrames(element);
  // DomDistiller will leave placeholders, which need to be replaced with
  // actual iframes.
  fillYouTubePlaceholders(element);
  sanitizeLinks(element);
  identifyEmptySVGs(element);
  ImageClassifier.processImagesIn(element);
  ListClassifier.processListsIn(element);
}

function addToPage(html) {
  const div = document.createElement('div');
  div.innerHTML = html;
  $('content').appendChild(div);
  postProcessElement(div);
}

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

  constructor() {
    // Baseline thresholds in density-independent units (CSS pixels).
    this.smallAreaUpperBoundDp = 64 * 64;
    this.inlineWidthFallbackUpperBoundDp = 300;

    // Matches common keywords for icons or mathematical formulas.
    const mathyKeywords =
        ['math', 'latex', 'equation', 'formula', 'tex', 'icon'];
    this._mathyKeywordsRegex =
        new RegExp('\\b(' + mathyKeywords.join('|') + ')\\b', 'i');

    // Matches characters commonly found in inline formulas.
    this._mathyAltTextRegex = /[+\-=_^{}\\]/;

    // Extracts the filename from a URL path.
    this._filenameRegex = /(?:.*\/)?([^?#]*)/;
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
   * Checks if the image is the primary content of its container.
   * @param {HTMLImageElement} img The image element to check.
   * @return {boolean} True if the image should be full-width.
   * @private
   */
  _isDefinitelyFullWidth(img) {
    // Image is in a <figure> with a <figcaption>.
    const parent = img.parentElement;
    if (parent && parent.tagName === 'FIGURE' &&
        parent.querySelector('figcaption')) {
      return true;
    }

    // Image is the only significant content in its container.
    let container = parent;
    while (container &&
           !['P', 'DIV', 'FIGURE', 'BODY'].includes(container.tagName)) {
      container = container.parentElement;
    }

    if (container) {
      for (const child of container.childNodes) {
        // Skip insignificant nodes.
        if (child === img) {
          continue;
        }
        if (child.tagName === 'BR') {
          continue;
        }
        if (child.nodeType === Node.TEXT_NODE &&
            child.textContent.trim() === '') {
          continue;
        }

        // If we reach this point, the node must be significant.
        return false;
      }
      // If we finish the loop, no significant siblings were found.
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
   * Determines an image's display style using a prioritized cascade of checks.
   * @param {HTMLImageElement} img The image element to classify.
   * @return {string} The CSS class to apply.
   */
  classify(img) {
    // Check for visually dominant images first, as this is the most reliable
    // signal and overrides all other heuristics.
    const renderedWidth = img.getBoundingClientRect().width;
    if (renderedWidth > 0 && window.innerWidth > 0 &&
        (renderedWidth / window.innerWidth) >
            ImageClassifier.DOMINANT_IMAGE_MIN_VIEWPORT_RATIO) {
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
   * Post-processes all images in an element to apply classification classes.
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

/**
 * A utility class to classify lists in distilled content.
 *
 * By default, list styling (bullets and numbering) is removed to avoid styling
 * navigational or UI elements. This class heuristically determines when to
 * restore styling for content lists.
 */
class ListClassifier {
  // The following thresholds are used in the content analysis stage for
  // ambiguous <ul> elements.

  // A high ratio (> this value) of items ending in punctuation often indicates
  // sentence-like content.
  static PUNCTUATION_RATIO_THRESHOLD = 0.5;

  // A `<ul>` where every `<li>` is a single link is considered content if
  // the average link text length is > this value.
  static AVG_LINK_TEXT_LENGTH_THRESHOLD = 15;

  // A low ratio (< this value) of link text to total text often indicates
  // a content list.
  static LINK_DENSITY_ACCEPTANCE_THRESHOLD = 0.5;

  // A high ratio (> this value) of link text to total text may indicate a
  // list of links.
  static LINK_DOMINANT_THRESHOLD = 0.5;

  // A selector for elements that are considered substantive content, used to
  // determine if a list item is more than just a simple link.
  static SUBSTANTIVE_ELEMENTS_SELECTOR =
      'p, div, img, h1, h2, h3, h4, h5, h6, table, pre, blockquote';

  constructor() {
    this.nonContentKeywords = new RegExp(
        'nav|menu|sidebar|footer|links|social|pagination|pager|breadcrumbs',
        'i');
    // Matches if a string is any single Unicode punctuation character.
    this.isPunctuation = new RegExp('^\\p{P}$', 'u');
  }

  /**
   * Checks for strong signals that a list is for navigation or UI.
   * @param {HTMLElement} list The list element.
   * @return {boolean} True if the list is navigational.
   * @private
   */
  _isNavigational(list) {
    // Check for explicit navigation roles or containing elements.
    if (list.closest(
            'nav, [role="navigation"], [role="menubar"], [role="menu"]')) {
      return true;
    }

    // Check for non-content keywords in id or class.
    const attributes = (list.id + ' ' + list.className).toLowerCase();
    if (this.nonContentKeywords.test(attributes)) {
      return true;
    }

    return false;
  }

  /**
   * Performs deeper content analysis on a <ul> list.
   * @param {HTMLElement} list The UL element.
   * @return {boolean} True if the list should be styled.
   * @private
   */
  _analyzeContent(list) {
    const listItems = list.querySelectorAll('li');
    const numListItems = listItems.length;

    if (numListItems === 0) {
      return false;
    }

    // In a single pass, collect all metrics needed for the heuristics.
    let itemsEndingWithPunctuation = 0;
    let totalTextLength = 0;
    let linkTextLength = 0;
    let hasSubstantiveElements = false;

    for (const li of listItems) {
      const itemText = li.textContent.trim();
      totalTextLength += itemText.length;

      if (itemText.length > 0 && this.isPunctuation.test(itemText.slice(-1))) {
        itemsEndingWithPunctuation++;
      }

      const links = li.querySelectorAll('a');
      for (const link of links) {
        // Don't trim() here; whitespace inside a link can be significant.
        linkTextLength += link.textContent.length;
      }

      if (!hasSubstantiveElements &&
          li.querySelector(ListClassifier.SUBSTANTIVE_ELEMENTS_SELECTOR)) {
        hasSubstantiveElements = true;
      }
    }

    // Heuristic A: A high punctuation ratio is a strong signal for
    // sentence-based content.
    const punctuationRatio = itemsEndingWithPunctuation / numListItems;
    if (punctuationRatio > ListClassifier.PUNCTUATION_RATIO_THRESHOLD) {
      return true;
    }

    const linkDensity =
        totalTextLength > 0 ? (linkTextLength / totalTextLength) : 0;

    // Heuristic B: A list that is dominated by links is content if the links
    // are long enough to be titles. This allows for some non-link text.
    if (!hasSubstantiveElements &&
        linkDensity > ListClassifier.LINK_DOMINANT_THRESHOLD) {
      const avgLinkTextLength =
          numListItems > 0 ? (linkTextLength / numListItems) : 0;
      return avgLinkTextLength >= ListClassifier.AVG_LINK_TEXT_LENGTH_THRESHOLD;
    }

    // Heuristic C: A list with a low density of links is likely content.
    if (linkDensity < ListClassifier.LINK_DENSITY_ACCEPTANCE_THRESHOLD) {
      return true;
    }

    // Default to false if undecided.
    return false;
  }

  /**
   * Main classification logic.
   * @param {HTMLElement} list The list element (ul or ol).
   * @return {boolean} True if the list should be styled.
   */
  classify(list) {
    // An empty list is never content.
    if (list.children.length === 0) {
      return false;
    }

    // Stage 1: Reject navigational lists immediately.
    if (this._isNavigational(list)) {
      return false;
    }

    // Stage 2: Accept lists that are clearly content.
    // An <ol> that isn't navigational is always considered content.
    if (list.tagName === 'OL') {
      return true;
    }
    // Any list with block-level elements is also considered content.
    if (list.querySelector('li p, li ul, li ol')) {
      return true;
    }

    // Stage 3: For remaining ambiguous <ul> elements, perform deeper analysis.
    // Other element types that reach this point are not considered content.
    if (list.tagName === 'UL') {
      return this._analyzeContent(list);
    }

    return false;
  }

  /**
   * Post-processes all lists in an element to apply classification classes.
   * @param {HTMLElement} element The element to search for lists in.
   */
  static processListsIn(element) {
    const classifier = new ListClassifier();
    const lists = element.querySelectorAll('ul, ol');
    for (const list of lists) {
      if (classifier.classify(list)) {
        list.classList.add('distilled-content-list');
      }
    }
  }
}

/**
 * Visits all links on the page, preserve http and https links and have them
 * open to new tab. Remove (i.e., unwrap) otherwise.
 * @param {HTMLElement} element The element to sanitize links in.
 */
function sanitizeLinks(element) {
  const allLinks = element.querySelectorAll('a');

  allLinks.forEach(linkElement => {
    const href = linkElement.getAttribute('href');

    if (href) {
      let keepLink = false;
      // Use a try-catch block to handle malformed URLs gracefully.
      try {
        if (href) {
          const url = new URL(href, window.location.href);
          // In particular, reject javascript: and #in-page links.
          if (url.protocol === 'http:' || url.protocol === 'https:') {
            keepLink = true;
            // Open to new tab.
            linkElement.target = '_blank';
          }
        }
      } catch (error) {
        // URL is malformed.
      }

      if (!keepLink) {
        // If the protocol is invalid or the URL is malformed, unwrap the link.
        const parent = linkElement.parentNode;

        if (parent) {
          // Iterate through the link's child nodes and move them to the parent.
          // Using a spread operator to create a copy, as childNodes is a live
          // list.
          [...linkElement.childNodes].forEach(node => {
            parent.insertBefore(node, linkElement);
          });

          // Remove the original anchor tag.
          linkElement.remove();
        }
      }
    }
    // With href, an anchor can be a placeholder. Leave these alone.
  });
}

/**
 * Finds SVGs that use a local resource pointer (e.g. <use xlink:href="#...")
 * and adds a class to them for styling. This is necessary because CSS
 * selectors for namespaced attributes like `xlink:href` are not reliably
 * supported across all renderers.
 * @param {HTMLElement} element The element to search for SVGs in.
 */
function identifyEmptySVGs(element) {
  const svgs = element.getElementsByTagName('svg');
  for (const svg of svgs) {
    const useElement = svg.querySelector('use');
    if (!useElement) {
      continue;
    }

    const href = useElement.getAttribute('href');
    const xlinkHref = useElement.getAttribute('xlink:href');

    if (href?.startsWith('#') || xlinkHref?.startsWith('#')) {
      svg.classList.add('distilled-svg-with-local-ref');
    }
  }
}

/**
 * Locates youtube embeds generated by DomDistiller, and creates an iframe for
 * each.
 * @param {HTMLElement} element The element to search for placeholders in.
 */
function fillYouTubePlaceholders(element) {
  const placeholders = element.getElementsByClassName('embed-placeholder');
  for (let i = 0; i < placeholders.length;
 i++) {
    if (!placeholders[i].hasAttribute('data-type') ||
        placeholders[i].getAttribute('data-type') !== 'youtube' ||
        !placeholders[i].hasAttribute('data-id')) {
      continue;
    }
    const embed = document.createElement('iframe');
    const url = 'http://www.youtube.com/embed/' +
        placeholders[i].getAttribute('data-id');
    embed.setAttribute('src', url);
    embed.setAttribute('type', 'text/html');
    embed.setAttribute('frameborder', '0');
    embedYoutubeIFrame(embed);
  }
}

/**
 * Locates existing youtube iframes and applies viewer stylings to them. This
 * is only relevant to readability which leave iframes in the result.
 * DomDistiller leaves behind placeholders, which are handled by
 * #fillYouTubePlaceholders.
 * @param {HTMLElement} element The element to search for iframes in.
 */
function addClassesToYoutubeIFrames(element) {
  const iframes = element.getElementsByTagName('iframe');
  for (let i = 0; i < iframes.length; i++) {
    const iframe = iframes[i];
    if (!isYouTubeIframe(iframe.src)) {
      continue;
    }
    embedYoutubeIFrame(iframe);
  }
}

/**
 * Checks if an iframe element is a YouTube video embed.
 * @param src The iframe element to check.
 * @returns True if the iframe is a YouTube video, false otherwise.
 */
function isYouTubeIframe(src) {
  try {
    const url = new URL(src);
    const hostname = url.hostname;

    // Check for standard youtube.com or the privacy-enhanced
    // youtube-nocookie.com
    return (
        hostname === 'www.youtube.com' || hostname === 'youtube.com' ||
        hostname === 'www.youtube-nocookie.com');
  } catch (error) {
    // Invalid URL in src, so it's not a valid YouTube embed
    return false;
  }
}

/**
 * Takes the given youtube iframe, adds a class and embeds it in a div. This is
 * used to apply consistent styling for all youtube embeds.
 * @param element The iframe element to embed within a container.
 */
function embedYoutubeIFrame(element) {
  const parent = element.parentElement;
  const container = document.createElement('div');
  element.setAttribute('class', 'youtubeIframe');
  container.setAttribute('class', 'youtubeContainer');
  parent.replaceChild(container, element);
  container.appendChild(element);
}

/**
 * Finds all tables within an element and wraps each in a div with the
 * 'scrollable-container' class to enable horizontal scrolling.
 * @param {HTMLElement} element The element to search for tables in.
*/
function wrapTables(element) {
  const containerClass = 'distilled-scrollable-container';
  const tables = element.querySelectorAll('table');
  tables.forEach(table => {
    const tableParent = table.parentElement;
    if (!tableParent || tableParent.classList.contains(containerClass)) {
      return;
    }

    const wrapper = document.createElement('div');
    wrapper.className = containerClass;

    tableParent.insertBefore(wrapper, table);
    wrapper.appendChild(table);
  });
}

function showLoadingIndicator(isLastPage) {
  $('loading-indicator').className = isLastPage ? 'hidden' : 'visible';
}

// Sets the title.
function setTitle(title, documentTitleSuffix) {
  $('title-holder').textContent = title;
  if (documentTitleSuffix) {
    document.title = title + documentTitleSuffix;
  } else {
    document.title = title;
  }
}

// Set the text direction of the document ('ltr', 'rtl', or 'auto').
function setTextDirection(direction) {
  document.body.setAttribute('dir', direction);
}

// Get the currently applied appearance setting.
function getAppearanceSetting(settingClasses) {
  const cls = Array.from(document.body.classList)
                  .find((cls) => settingClasses.includes(cls));
  return cls ? cls : settingClasses[0];
}

function useTheme(theme) {
  settingsDialog.useTheme(theme);
}

function useFontFamily(fontFamily) {
  settingsDialog.useFontFamily(fontFamily);
}

function updateToolbarColor(theme) {
  let toolbarColor;
  if (theme === 'sepia') {
    toolbarColor = '#BF9A73';
  } else if (theme === 'dark') {
    toolbarColor = '#1A1A1A';
  } else {
    toolbarColor = '#F5F5F5';
  }
  $('theme-color').content = toolbarColor;
}

function maybeSetWebFont() {
  // On iOS, the web fonts block the rendering until the resources are
  // fetched, which can take a long time on slow networks.
  // In Blink, it times out after 3 seconds and uses fallback fonts.
  // See crbug.com/711650
  if (distillerOnIos) {
    return;
  }

  const e = document.createElement('link');
  e.href = 'https://fonts.googleapis.com/css?family=Roboto';
  e.rel = 'stylesheet';
  e.type = 'text/css';
  document.head.appendChild(e);
}

// TODO(crbug.com/40108835): Consider making this a custom HTML element.
class FontSizeSlider {
  constructor() {
    this.element = $('font-size-selection');
    this.baseSize = 16;
    // These scales are applied to a base size of 16px.
    this.fontSizeScale = [0.875, 0.9375, 1, 1.125, 1.25, 1.5, 1.75, 2, 2.5, 3];

    this.element.addEventListener('input', (e) => {
      const scale = this.fontSizeScale[e.target.value];
      this.useFontScaling(scale);
      distiller.storeFontScalingPref(parseFloat(scale));
    });

    this.tickmarks = document.createElement('datalist');
    this.tickmarks.setAttribute('class', 'tickmarks');
    this.element.after(this.tickmarks);

    for (let i = 0; i < this.fontSizeScale.length; i++) {
      const option = document.createElement('option');
      option.setAttribute('value', i);
      option.textContent = this.fontSizeScale[i] * this.baseSize;
      this.tickmarks.appendChild(option);
    }
    this.element.value = 2;
    this.update(this.element.value);
  }
  // TODO(meredithl): validate |scale| and snap to nearest supported font size.
  useFontScaling(scale, restoreCenter = true) {
    this.element.value = this.fontSizeScale.indexOf(scale);
    document.documentElement.style.fontSize = scale * this.baseSize + 'px';
    this.update(this.element.value);
  }

  update(position) {
    this.element.style.setProperty(
        '--fontSizePercent',
        (position / (this.fontSizeScale.length - 1) * 100) + '%');
    this.element.setAttribute(
        'aria-valuetext', this.fontSizeScale[position] + 'px');
    for (let option = this.tickmarks.firstChild; option != null;
         option = option.nextSibling) {
      const isBeforeThumb = option.value < position;
      option.classList.toggle('before-thumb', isBeforeThumb);
      option.classList.toggle('after-thumb', !isBeforeThumb);
    }
  }

  useBaseFontSize(size) {
    this.baseSize = size;
    this.update(this.element.value);
  }
}

maybeSetWebFont();

// The zooming speed relative to pinching speed.
const FONT_SCALE_MULTIPLIER = 0.5;

const MIN_SPAN_LENGTH = 20;

class Pincher {
  // When users pinch in Reader Mode, the page would zoom in or out as if it
  // is a normal web page allowing user-zoom. At the end of pinch gesture, the
  // page would do text reflow. These pinch-to-zoom and text reflow effects
  // are not native, but are emulated using CSS and JavaScript.
  //
  // In order to achieve near-native zooming and panning frame rate, fake 3D
  // transform is used so that the layer doesn't repaint for each frame.
  //
  // After the text reflow, the web content shown in the viewport should
  // roughly be the same paragraph before zooming.
  //
  // The control point of font size is the html element, so that both "em" and
  // "rem" are adjusted.
  //
  // TODO(wychen): Improve scroll position when elementFromPoint is body.

  constructor() {
    // This has to be in sync with largest 'font-size' in distilledpage_{}.css.
    // This value is hard-coded because JS might be injected before CSS is
    // ready. See crbug.com/1004663.
    this.baseSize = 16;
    this.pinching = false;
    this.fontSizeAnchor = 1.0;

    this.focusElement = null;
    this.focusPos = 0;
    this.initClientMid = null;

    this.clampedScale = 1.0;

    this.lastSpan = null;
    this.lastClientMid = null;

    this.scale = 1.0;
    this.shiftX = 0;
    this.shiftY = 0;

    window.addEventListener('touchstart', (e) => {
      this.handleTouchStart(e);
    }, {passive: false});
    window.addEventListener('touchmove', (e) => {
      this.handleTouchMove(e);
    }, {passive: false});
    window.addEventListener('touchend', (e) => {
      this.handleTouchEnd(e);
    }, {passive: false});
    window.addEventListener('touchcancel', (e) => {
      this.handleTouchCancel(e);
    }, {passive: false});
  }

  /** @private */
  refreshTransform_() {
    const slowedScale = Math.exp(Math.log(this.scale) * FONT_SCALE_MULTIPLIER);
    this.clampedScale =
        Math.max($MIN_SCALE, Math.min($MAX_SCALE, this.fontSizeAnchor * slowedScale));

    // Use "fake" 3D transform so that the layer is not repainted.
    // With 2D transform, the frame rate would be much lower.
    // clang-format off
    document.body.style.transform =
        'translate3d(' + this.shiftX + 'px,'
                       + this.shiftY + 'px, 0px)' +
        'scale(' + this.clampedScale / this.fontSizeAnchor + ')';
    // clang-format on
  }

  /** @private */
  saveCenter_(clientMid) {
    // Try to preserve the pinching center after text reflow.
    // This is accurate to the HTML element level.
    this.focusElement = document.elementFromPoint(clientMid.x, clientMid.y);
    const rect = this.focusElement.getBoundingClientRect();
    this.initClientMid = clientMid;
    this.focusPos =
        (this.initClientMid.y - rect.top) / (rect.bottom - rect.top);
  }

  /** @private */
  restoreCenter_() {
    const rect = this.focusElement.getBoundingClientRect();
    const targetTop = this.focusPos * (rect.bottom - rect.top) + rect.top +
        document.scrollingElement.scrollTop -
        (this.initClientMid.y + this.shiftY);
    document.scrollingElement.scrollTop = targetTop;
  }

  /** @private */
  endPinch_() {
    this.pinching = false;

    document.body.style.transformOrigin = '';
    document.body.style.transform = '';
    document.documentElement.style.fontSize =
        this.clampedScale * this.baseSize + 'px';

    this.restoreCenter_();

    let img = $('fontscaling-img');
    if (!img) {
      img = document.createElement('img');
      img.id = 'fontscaling-img';
      img.style.display = 'none';
      document.body.appendChild(img);
    }
    img.src = '/savefontscaling/' + this.clampedScale;
  }

  /** @private */
  touchSpan_(e) {
    const count = e.touches.length;
    const mid = this.touchClientMid_(e);
    let sum = 0;
    for (let i = 0; i < count; i++) {
      const dx = (e.touches[i].clientX - mid.x);
      const dy = (e.touches[i].clientY - mid.y);
      sum += Math.hypot(dx, dy);
    }
    // Avoid very small span.
    return Math.max(MIN_SPAN_LENGTH, sum / count);
  }

  /** @private */
  touchClientMid_(e) {
    const count = e.touches.length;
    let sumX = 0;
    let sumY = 0;
    for (let i = 0; i < count; i++) {
      sumX += e.touches[i].clientX;
      sumY += e.touches[i].clientY;
    }
    return {x: sumX / count, y: sumY / count};
  }

  /** @private */
  touchPageMid_(e) {
    const clientMid = this.touchClientMid_(e);
    return {
      x: clientMid.x - e.touches[0].clientX + e.touches[0].pageX,
      y: clientMid.y - e.touches[0].clientY + e.touches[0].pageY,
    };
  }

  handleTouchStart(e) {
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length > 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;

    this.pinching = true;
    this.fontSizeAnchor =
        parseFloat(getComputedStyle(document.documentElement).fontSize) /
        this.baseSize;

    const pinchOrigin = this.touchPageMid_(e);
    document.body.style.transformOrigin =
        pinchOrigin.x + 'px ' + pinchOrigin.y + 'px';

    this.saveCenter_(clientMid);

    this.lastSpan = span;
    this.lastClientMid = clientMid;

    this.refreshTransform_();
  }

  handleTouchMove(e) {
    if (!this.pinching) {
      return;
    }
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    this.scale *= this.touchSpan_(e) / this.lastSpan;
    this.shiftX += clientMid.x - this.lastClientMid.x;
    this.shiftY += clientMid.y - this.lastClientMid.y;

    this.refreshTransform_();

    this.lastSpan = span;
    this.lastClientMid = clientMid;
  }

  handleTouchEnd(e) {
    if (!this.pinching) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length >= 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.endPinch_();
  }

  handleTouchCancel(e) {
    if (!this.pinching) {
      return;
    }
    this.endPinch_();
  }

  reset() {
    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;
    this.clampedScale = 1;
    document.documentElement.style.fontSize =
        this.clampedScale * this.baseSize + 'px';
  }

  status() {
    return {
      scale: this.scale,
      clampedScale: this.clampedScale,
      shiftX: this.shiftX,
      shiftY: this.shiftY,
    };
  }

  useFontScaling(scaling, restoreCenter = true) {
    if (restoreCenter) {
      this.saveCenter_({x: window.innerWidth / 2, y: window.innerHeight / 2});
    }
    this.shiftX = 0;
    this.shiftY = 0;
    document.documentElement.style.fontSize = scaling * this.baseSize + 'px';
    this.clampedScale = scaling;
    if (restoreCenter) {
      this.restoreCenter_();
    }
  }

  useBaseFontSize(size) {
    this.baseSize = size;
    this.reset();
  }
}

// The pincher is only defined on Android, and the font size slider only on
// desktop.
// eslint-disable-next-line no-var
var pincher, fontSizeSlider;
if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
  pincher = new Pincher();
} else {
  fontSizeSlider = new FontSizeSlider();
}

/**
 * Called to set the baseFontSize for the pinch/slider (whichever is active).
 */
function useBaseFontSize(size) {
  if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
    pincher.useBaseFontSize(size);
  } else {
    fontSizeSlider.useBaseFontSize(size);
  }
}

function useFontScaling(scale, restoreCenter = true) {
  if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
    pincher.useFontScaling(scale, restoreCenter);
  } else {
    fontSizeSlider.useFontScaling(scale, restoreCenter);
  }
}

/**
 * Finds a paragraph with `innerText` matching `hash` and `charCount`, then
 * scrolls to that paragraph with the provided `progress` corresponding to the
 * location to scroll to wrt. that paragraph, 0 being the top of that
 * paragraph, 1 being the bottom.
 * @param {number} hash The hash of the paragraph's innerText.
 * @param {number} charCount The character count of the paragraph's innerText.
 * @param {number} progress The scroll progress within the paragraph (0-1).
 */
function scrollToParagraphByHash(hash, charCount, progress) {
  const targetHash = hash;
  const targetCharCount = charCount;
  const paragraphs = document.querySelectorAll('p');
  for (let i = 0; i < paragraphs.length; i++) {
    const p = paragraphs[i];
    const pText = p.innerText;
    if (pText.length === targetCharCount) {
      // Only compute hash if the length already matches.
      const hashCode = (s) =>
          s.split('').reduce((a, b) => ((a << 5) - a + b.charCodeAt(0)) | 0, 0);
      const pHash = hashCode(pText);
      if (pHash === targetHash) {
        const rect = p.getBoundingClientRect();
        const scrollOffset = (window.scrollY + rect.top) +
            (rect.height * progress) - (window.innerHeight / 2);
        window.scrollTo(0, scrollOffset);
        break;
      }
    }
  }
}

class SettingsDialog {
  constructor(
      toggleElement, dialogElement, backdropElement, themeFieldset,
      fontFamilySelect) {
    this._toggleElement = toggleElement;
    this._dialogElement = dialogElement;
    this._backdropElement = backdropElement;
    this._themeFieldset = themeFieldset;
    this._fontFamilySelect = fontFamilySelect;

    this._toggleElement.addEventListener('click', this.toggle.bind(this));
    this._dialogElement.addEventListener('close', this.close.bind(this));
    this._backdropElement.addEventListener('click', this.close.bind(this));

    $('close-settings-button').addEventListener('click', this.close.bind(this));

    this._themeFieldset.addEventListener('change', (e) => {
      const newTheme = e.target.value;
      this.useTheme(newTheme);
      distiller.storeThemePref(themeClasses.indexOf(newTheme));
    });

    this._fontFamilySelect.addEventListener('change', (e) => {
      const newFontFamily = e.target.value;
      this.useFontFamily(newFontFamily);
      distiller.storeFontFamilyPref(fontFamilyClasses.indexOf(newFontFamily));
    });

    // Appearance settings are loaded from user preferences, so on page load
    // the controllers for these settings may need to be updated to reflect
    // the active setting.
    this._updateFontFamilyControls(getAppearanceSetting(fontFamilyClasses));
    const selectedTheme = getAppearanceSetting(themeClasses);
    this._updateThemeControls(selectedTheme);
    updateToolbarColor(selectedTheme);
  }

  toggle() {
    if (this._dialogElement.open) {
      this.close();
    } else {
      this.showModal();
    }
  }

  showModal() {
    this._toggleElement.classList.add('activated');
    this._backdropElement.style.display = 'block';
    this._dialogElement.showModal();
  }

  close() {
    this._toggleElement.classList.remove('activated');
    this._backdropElement.style.display = 'none';
    this._dialogElement.close();
  }

  useTheme(theme) {
    themeClasses.forEach(
        (element) =>
            document.body.classList.toggle(element, element === theme));
    this._updateThemeControls(theme);
    updateToolbarColor(theme);
  }

  _updateThemeControls(theme) {
    const queryString = `input[value=${theme}]`;
    this._themeFieldset.querySelector(queryString).checked = true;
  }

  useFontFamily(fontFamily) {
    fontFamilyClasses.forEach(
        (element) =>
            document.body.classList.toggle(element, element === fontFamily));
    this._updateFontFamilyControls(fontFamily);
  }

  _updateFontFamilyControls(fontFamily) {
    this._fontFamilySelect.selectedIndex =
        fontFamilyClasses.indexOf(fontFamily);
  }
}

const settingsDialog = new SettingsDialog(
    $('settings-toggle'), $('settings-dialog'), $('dialog-backdrop'),
    $('theme-selection'), $('font-family-selection'));
