// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(JSThemesAndFonts)

// These classes must agree with the font classes in distilledpage_common.css.
const themeClasses = ['light', 'dark', 'sepia'];
const fontFamilyClasses = ['sans-serif', 'serif', 'monospace', 'Lexend'];

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
  SettingsDialog.getInstance().useTheme(theme);
}

function useFontFamily(fontFamily) {
  SettingsDialog.getInstance().useFontFamily(fontFamily);
}

function setLinksEnabled(enabled) {
  document.body.classList.toggle('links-hidden', !enabled);
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

/**
 * Called to set the baseFontSize for the pinch/slider (whichever is active).
 */
function useBaseFontSize(size) {
  if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
    Pincher.getInstance().useBaseFontSize(size);
  } else {
    FontSizeSlider.getInstance().useBaseFontSize(size);
  }
}

function useFontScaling(scale, restoreCenter = true) {
  if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
    Pincher.getInstance().useFontScaling(scale, restoreCenter);
  } else {
    FontSizeSlider.getInstance().useFontScaling(scale, restoreCenter);
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

/**
 * Initializes the Dom Distiller viewer UI components based on the platform.
 * This function should be called after the DOM is loaded.
 */
function initializeDomDistillerViewer() {
  // The settings dialog is always present.
  SettingsDialog.getInstance();
  if (navigator.userAgent.toLowerCase().indexOf('android') > -1) {
    Pincher.getInstance();
  } else {
    FontSizeSlider.getInstance();
  }
}
