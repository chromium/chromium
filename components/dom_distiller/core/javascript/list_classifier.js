// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
