// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {$} from 'chrome://resources/js/util.js';

const detectionLogs = [];

/**
 * Initializes UI and sends a message to the browser for
 * initialization.
 */
function initialize() {
  addMessageHandlers();
  const tabbox = document.querySelector('cr-tab-box');
  tabbox.hidden = false;
  chrome.send('requestInfo');

  const button = $('detection-logs-dump');
  button.addEventListener('click', onDetectionLogsDump);

  const tabpanelNodeList = document.querySelectorAll('div[slot=\'panel\']');
  const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
  const tabpanelIds = tabpanels.map(function(tab) {
    return tab.id;
  });

  tabbox.addEventListener('selected-index-change', e => {
    const tabpanel = tabpanels[e.detail];
    const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
    window.location.hash = hash;
  });

  const activateTabByHash = function() {
    let hash = window.location.hash;

    // Remove the first character '#'.
    hash = hash.substring(1);

    const id = 'tabpanel-' + hash;
    const index = tabpanelIds.indexOf(id);
    if (index === -1) {
      return;
    }
    tabbox.setAttribute('selected-index', `${index}`);
  };

  window.onhashchange = activateTabByHash;
  activateTabByHash();
}

/*
 * Creates a button to dismiss an item.
 *
 * @param {Function} func Callback called when the button is clicked.
 */
function createDismissingButton(func) {
  const button = document.createElement('button');
  button.textContent = 'X';
  button.classList.add('dismissing');
  button.addEventListener('click', function(e) {
    e.preventDefault();
    func();
  }, false);
  return button;
}

/**
 * Creates a new LI element with a button to dismiss the item.
 *
 * @param {string} text The label of the LI element.
 * @param {Function} func Callback called when the button is clicked.
 */
function createLIWithDismissingButton(text, func) {
  const span = document.createElement('span');
  span.textContent = text;

  const li = document.createElement('li');
  li.appendChild(span);
  li.appendChild(createDismissingButton(func));
  return li;
}

/**
 * Formats the language name to a human-readable text. For example, if
 * |langCode| is 'en', this may return 'en (English)'.
 *
 * @param {string} langCode ISO 639 language code.
 * @return {string} The formatted string.
 */
function formatLanguageCode(langCode) {
  const key = 'language-' + langCode;
  if (loadTimeData.valueExists(key)) {
    const langName = loadTimeData.getString(key);
    return langCode + ' (' + langName + ')';
  }

  return langCode;
}

/**
 * Formats the error type to a human-readable text.
 *
 * @param {string} error Translation error type from the browser.
 * @return {string} The formatted string.
 */
function formatTranslateErrorsType(error) {
  // This list is from chrome/common/translate/translate_errors.h.
  // If this header file is updated, the below list also should be updated.
  const errorStrs = {
    0: 'None',
    1: 'Network',
    2: 'Initialization Error',
    3: 'Unknown Language',
    4: 'Unsupported Language',
    5: 'Identical Languages',
    6: 'Translation Error',
    7: 'Translation Timeout',
    8: 'Unexpected Script Error',
    9: 'Bad Origin',
    10: 'Script Load Error',
  };

  if (error < 0 || errorStrs.length <= error) {
    console.error('Invalid error code:', error);
    return 'Invalid Error Code';
  }
  return errorStrs[error];
}

/**
 * @return {TrustedHTML|string} Empty TrustedHTML or empty string based on
 * Trusted Types support.
 */
function emptyHTML() {
  if (window.trustedTypes) {
    return trustedTypes.emptyHTML;
  } else {
    return '';
  }
}

/**
 * Handles the message of 'prefsUpdated' from the browser.
 *
 * @param {Object} detail the object which represents pref values.
 */
function onPrefsUpdated(detail) {
  let ul;

  ul = document.querySelector('#prefs-blocked-languages ul');
  ul.innerHTML = emptyHTML();

  if ('translate_blocked_languages' in detail) {
    const langs = detail['translate_blocked_languages'];

    langs.forEach(function(langCode) {
      const text = formatLanguageCode(langCode);

      const li = createLIWithDismissingButton(text, function() {
        chrome.send('removePrefItem', ['blocked_languages', langCode]);
      });
      ul.appendChild(li);
    });
  }

  ul = document.querySelector('#prefs-site-blocklist ul');
  ul.innerHTML = emptyHTML();

  if ('translate_site_blocklist' in detail) {
    const sites = detail['translate_site_blocklist'];

    sites.forEach(function(site) {
      const li = createLIWithDismissingButton(site, function() {
        chrome.send('removePrefItem', ['site_blocklist', site]);
      });
      ul.appendChild(li);
    });
  }

  ul = document.querySelector('#prefs-allowlists ul');
  ul.innerHTML = emptyHTML();

  if ('translate_allowlists' in detail) {
    const pairs = detail['translate_allowlists'];

    Object.keys(pairs).forEach(function(fromLangCode) {
      const toLangCode = pairs[fromLangCode];
      const text = formatLanguageCode(fromLangCode) + ' \u2192 ' +
          formatLanguageCode(toLangCode);

      const li = createLIWithDismissingButton(text, function() {
        chrome.send('removePrefItem', ['allowlists', fromLangCode, toLangCode]);
      });
      ul.appendChild(li);
    });
  }

  if ('translate_recent_target' in detail) {
    const recentTarget = detail['translate_recent_target'];

    const p = $('recent-override');

    p.innerHTML = emptyHTML();

    appendTextFieldWithButton(p, recentTarget, function(value) {
      chrome.send('setRecentTargetLanguage', [value]);
    });
  }

  const p = document.querySelector('#prefs-dump p');
  const content = JSON.stringify(detail, null, 2);
  p.textContent = content;
}

/**
 * Handles the message of 'supportedLanguagesUpdated' from the browser.
 *
 * @param {Object} details the object which represents the supported
 *     languages by the Translate server.
 */
function onSupportedLanguagesUpdated(details) {
  const span =
      $('prefs-supported-languages-last-updated').querySelector('span');
  span.textContent = formatDate(new Date(details['last_updated']));

  const ul = $('prefs-supported-languages-languages');
  ul.innerHTML = emptyHTML();
  const languages = details['languages'];
  for (let i = 0; i < languages.length; i++) {
    const language = languages[i];
    const li = document.createElement('li');

    const text = formatLanguageCode(language);
    li.innerText = text;

    ul.appendChild(li);
  }
}

/**
 * Handles the message of 'countryUpdated' from the browser.
 *
 * @param {Object} details the object containing the country
 *     information.
 */
function onCountryUpdated(details) {
  const p = $('country-override');

  p.innerHTML = emptyHTML();

  const country = details['country'] || '';
  const h2 = $('override-variations-country');
  h2.title =
      ('Changing this value will override the permanent country stored ' +
       'by variations. The overridden country is not automatically ' +
       'updated when Chrome is updated and overrides all variations ' +
       'country settings. After clicking clear button or the overridden ' +
       'country is not set, the text box shows the country used by ' +
       'permanent consistency studies. The value that this is overriding ' +
       'gets automatically updated with a new value received from the ' +
       'variations server when Chrome is updated.');

  // Add the button to override the country. Note: The country-override
  // component is re-created on country update, so there is no issue of this
  // being called multiple times.
  appendTextFieldWithButton(p, country, function(value) {
    chrome.send('overrideCountry', [value]);
  });

  appendClearButton(
      p, 'overridden' in details && details['overridden'], function() {
        chrome.send('overrideCountry', ['']);
      });

  if ('update' in details && details['update']) {
    const div1 = document.createElement('div');
    div1.textContent = 'Permanent stored country updated.';
    const div2 = document.createElement('div');
    div2.textContent =
        ('You will need to restart your browser ' +
         'for the changes to take effect.');
    p.appendChild(div1);
    p.appendChild(div2);
  }
}

/**
 * Adds '0's to |number| as a string. |width| is length of the string
 * including '0's.
 *
 * @param {string} number The number to be converted into a string.
 * @param {number} width The width of the returned string.
 * @return {string} The formatted string.
 */
function padWithZeros(number, width) {
  const numberStr = number.toString();
  const restWidth = width - numberStr.length;
  if (restWidth <= 0) {
    return numberStr;
  }

  return Array(restWidth + 1).join('0') + numberStr;
}

/**
 * Formats |date| as a Date object into a string. The format is like
 * '2006-01-02 15:04:05'.
 *
 * @param {Date} date Date to be formatted.
 * @return {string} The formatted string.
 */
function formatDate(date) {
  const year = date.getFullYear();
  const month = date.getMonth() + 1;
  const day = date.getDate();
  const hour = date.getHours();
  const minute = date.getMinutes();
  const second = date.getSeconds();

  const yearStr = padWithZeros(year, 4);
  const monthStr = padWithZeros(month, 2);
  const dayStr = padWithZeros(day, 2);
  const hourStr = padWithZeros(hour, 2);
  const minuteStr = padWithZeros(minute, 2);
  const secondStr = padWithZeros(second, 2);

  const str = yearStr + '-' + monthStr + '-' + dayStr + ' ' + hourStr + ':' +
      minuteStr + ':' + secondStr;

  return str;
}

/**
 * Appends a new TD element to the specified element.
 *
 * @param {string} parent The element to which a new TD element is appended.
 * @param {string} content The text content of the element.
 * @param {string} className The class name of the element.
 */
function appendTD(parent, content, className) {
  const td = document.createElement('td');
  td.textContent = content;
  td.className = className;
  parent.appendChild(td);
}

function appendBooleanTD(parent, value, className) {
  const td = document.createElement('td');
  td.textContent = value;
  td.className = className;
  td.bgColor = value ? '#3cba54' : '#db3236';
  parent.appendChild(td);
}

/**
 * Handles the message of 'languageDetectionInfoAdded' from the
 * browser.
 *
 * @param {Object} detail The object which represents the logs.
 */
function onLanguageDetectionInfoAdded(detail) {
  detectionLogs.push(detail);

  const tr = document.createElement('tr');

  // If language detection was skipped, do not populate related details.
  const hasRunLangDetection = JSON.parse(detail['has_run_lang_detection']);
  const cldLang = hasRunLangDetection ?
      formatLanguageCode(detail['model_detected_language']) :
      'No page content - language detection skipped';
  const modelVersion =
      hasRunLangDetection ? detail['detection_model_version'] : '';
  const reliabilityScore =
      hasRunLangDetection ? detail['model_reliability_score'].toFixed(2) : '';
  const isReliable = hasRunLangDetection ? detail['is_model_reliable'] : '';

  appendTD(tr, formatDate(new Date(detail['time'])), 'detection-logs-time');
  appendTD(tr, detail['url'], 'detection-logs-url');
  appendTD(
      tr, formatLanguageCode(detail['content_language']),
      'detection-logs-content-language');
  appendTD(
      tr, formatLanguageCode(detail['html_root_language']),
      'detection-logs-html-root-language');
  appendTD(tr, cldLang, 'detection-logs-cld-language');
  appendTD(tr, modelVersion, 'detection-logs-detection-model-version');
  appendTD(tr, reliabilityScore, 'detection-logs-model-reliability');
  appendTD(tr, isReliable, 'detection-logs-is-cld-reliable');
  appendTD(tr, detail['has_notranslate'], 'detection-logs-has-notranslate');
  appendTD(
      tr, formatLanguageCode(detail['adopted_language']),
      'detection-logs-adopted-language');
  appendTD(tr, formatLanguageCode(detail['content']), 'detection-logs-content');

  // TD (and TR) can't use the CSS property 'max-height', so DIV
  // in the content is needed.
  const contentTD = tr.querySelector('.detection-logs-content');
  const div = document.createElement('div');
  div.textContent = contentTD.textContent;
  contentTD.textContent = '';
  contentTD.appendChild(div);

  const tabpanel = $('tabpanel-detection-logs');
  const tbody = tabpanel.getElementsByTagName('tbody')[0];
  tbody.appendChild(tr);
}

/**
 * Handles the message of 'translateErrorDetailsAdded' from the
 * browser.
 *
 * @param {Object} details The object which represents the logs.
 */
function onTranslateErrorDetailsAdded(details) {
  const tr = document.createElement('tr');

  appendTD(tr, formatDate(new Date(details['time'])), 'error-logs-time');
  appendTD(tr, details['url'], 'error-logs-url');
  appendTD(
      tr, details['error'] + ': ' + formatTranslateErrorsType(details['error']),
      'error-logs-error');

  const tabpanel = $('tabpanel-error-logs');
  const tbody = tabpanel.getElementsByTagName('tbody')[0];
  tbody.appendChild(tr);
}

/**
 * Handles the message of 'translateInitDetailsAdded' from the
 * browser.
 *
 * @param {Object} details The object which represents the logs.
 */
function onTranslateInitDetailsAdded(details) {
  const tr = document.createElement('tr');

  appendTD(tr, formatDate(new Date(details['time'])), 'init-logs-time');
  appendTD(tr, details['url'], 'init-logs-url');

  appendTD(tr, details['page_language_code'], 'init-logs-page-language-code');
  appendTD(tr, details['target_lang'], 'init-logs-target-lang');

  appendBooleanTD(tr, details['ui_shown'], 'init-logs-ui-shown');

  appendBooleanTD(
      tr, details['can_auto_translate'], 'init-logs-can-auto-translate');
  appendBooleanTD(tr, details['can_show_ui'], 'init-logs-can-show-ui');
  appendBooleanTD(
      tr, details['can_auto_href_translate'],
      'init-logs-can-auto-href-translate');
  appendBooleanTD(
      tr, details['can_show_href_translate_ui'],
      'init-logs-can-show-href-translate-ui');
  appendBooleanTD(
      tr, details['can_show_predefined_language_translate_ui'],
      'init-logs-can-show-predefined-language-translate-ui');
  appendBooleanTD(
      tr, details['should_suppress_from_ranker'],
      'init-logs-should-suppress-from-ranker');
  appendBooleanTD(
      tr, details['is_triggering_possible'],
      'init-logs-is-triggering-possible');
  appendBooleanTD(
      tr, details['should_auto_translate'], 'init-logs-should-auto-translate');
  appendBooleanTD(tr, details['should_show_ui'], 'init-logs-should-show-ui');

  appendTD(
      tr, details['auto_translate_target'], 'init-logs-auto-translate-target');
  appendTD(
      tr, details['href_translate_target'], 'init-logs-href-translate-target');
  appendTD(
      tr, details['predefined_translate_target'],
      'init-logs-predefined-translate-target');

  const tabpanel = $('tabpanel-init-logs');
  const tbody = tabpanel.getElementsByTagName('tbody')[0];
  tbody.appendChild(tr);
}


/**
 * Handles the message of 'translateEventDetailsAdded' from the browser.
 *
 * @param {Object} details The object which contains event information.
 */
function onTranslateEventDetailsAdded(details) {
  const tr = document.createElement('tr');
  appendTD(tr, formatDate(new Date(details['time'])), 'event-logs-time');
  appendTD(
      tr, details['filename'] + ': ' + details['line'], 'event-logs-place');
  appendTD(tr, details['message'], 'event-logs-message');

  const tbody = $('tabpanel-event-logs').getElementsByTagName('tbody')[0];
  tbody.appendChild(tr);
}

/**
 * Appends an <input type="text" /> and a button to `elt`, and sets the value
 * of the <input> to `value`. When the button is clicked,
 * `buttonClickCallback` is called with the value of the <input> field.
 *
 * @param {HTMLElement} elt Container element to append to.
 * @param {string} value Initial value of the <input> element.
 * @param {Function} buttonClickCallback Function to call when the button is
 *                                       clicked.
 */
function appendTextFieldWithButton(elt, value, buttonClickCallback) {
  const input = document.createElement('input');
  input.type = 'text';
  input.value = value;

  const button = document.createElement('button');
  button.textContent = 'update';
  button.addEventListener('click', function() {
    buttonClickCallback(input.value);
  }, false);

  elt.appendChild(input);
  elt.appendChild(document.createElement('br'));
  elt.appendChild(button);
}

/**
 * Appends a clear button to `elt`. When the button is clicked,
 * `clearCallback` is called.
 *
 * @param {HTMLElement} elt Container element to append to.
 * @param {boolean} enabled Whether the button is clickable.
 * @param {Function} clearCallback Function to call when the button is
 *                                 clicked.
 */
function appendClearButton(elt, enabled, clearCallback) {
  const button = document.createElement('button');
  button.textContent = 'clear';
  button.style.marginLeft = '10px';
  button.disabled = !enabled;
  if (enabled) {
    button.addEventListener('click', clearCallback, false);
  } else {
    // Used for tooltip when the button is unclickable.
    button.title = 'No pref values to clear.';
  }

  elt.appendChild(button);
}

function addMessageHandlers() {
  addWebUiListener('languageDetectionInfoAdded', onLanguageDetectionInfoAdded);
  addWebUiListener('prefsUpdated', onPrefsUpdated);
  addWebUiListener('supportedLanguagesUpdated', onSupportedLanguagesUpdated);
  addWebUiListener('countryUpdated', onCountryUpdated);
  addWebUiListener('translateErrorDetailsAdded', onTranslateErrorDetailsAdded);
  addWebUiListener('translateEventDetailsAdded', onTranslateEventDetailsAdded);
  addWebUiListener('translateInitDetailsAdded', onTranslateInitDetailsAdded);
}

/**
 * The callback of button#detetion-logs-dump.
 */
function onDetectionLogsDump() {
  const data = JSON.stringify(detectionLogs);
  const blob = new Blob([data], {'type': 'text/json'});
  const url = URL.createObjectURL(blob);
  const filename = 'translate_internals_detect_logs_dump.json';

  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);

  const event = document.createEvent('MouseEvent');
  event.initMouseEvent(
      'click', true, true, window, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, null);
  a.dispatchEvent(event);
}

document.addEventListener('DOMContentLoaded', initialize);
