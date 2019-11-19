// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

cr.define('cr.translateInternals', function() {
  const detectionLogs = [];

  /**
   * Initializes UI and sends a message to the browser for
   * initialization.
   */
  function initialize() {
    cr.ui.decorate('tabbox', cr.ui.TabBox);
    chrome.send('requestInfo');

    const button = $('detection-logs-dump');
    button.addEventListener('click', onDetectionLogsDump);

    const tabpanelNodeList = document.getElementsByTagName('tabpanel');
    const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
    const tabpanelIds = tabpanels.map(function(tab) {
      return tab.id;
    });

    const tabNodeList = document.getElementsByTagName('tab');
    const tabs = Array.prototype.slice.call(tabNodeList, 0);
    tabs.forEach(function(tab) {
      tab.onclick = function(e) {
        const tabbox = document.querySelector('tabbox');
        const tabpanel = tabpanels[tabbox.selectedIndex];
        const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
        window.location.hash = hash;
      };
    });

    const activateTabByHash = function() {
      let hash = window.location.hash;

      // Remove the first character '#'.
      hash = hash.substring(1);

      const id = 'tabpanel-' + hash;
      if (tabpanelIds.indexOf(id) == -1) {
        return;
      }

      $(id).selected = true;
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
   * Handles the message of 'prefsUpdated' from the browser.
   *
   * @param {Object} detail the object which represents pref values.
   */
  function onPrefsUpdated(detail) {
    let ul;

    ul = document.querySelector('#prefs-blocked-languages ul');
    ul.innerHTML = '';

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

    ul = document.querySelector('#prefs-language-blacklist ul');
    ul.innerHTML = '';

    if ('translate_language_blacklist' in detail) {
      const langs = detail['translate_language_blacklist'];

      langs.forEach(function(langCode) {
        const text = formatLanguageCode(langCode);

        const li = createLIWithDismissingButton(text, function() {
          chrome.send('removePrefItem', ['language_blacklist', langCode]);
        });
        ul.appendChild(li);
      });
    }

    ul = document.querySelector('#prefs-site-blacklist ul');
    ul.innerHTML = '';

    if ('translate_site_blacklist' in detail) {
      const sites = detail['translate_site_blacklist'];

      sites.forEach(function(site) {
        const li = createLIWithDismissingButton(site, function() {
          chrome.send('removePrefItem', ['site_blacklist', site]);
        });
        ul.appendChild(li);
      });
    }

    ul = document.querySelector('#prefs-whitelists ul');
    ul.innerHTML = '';

    if ('translate_whitelists' in detail) {
      const pairs = detail['translate_whitelists'];

      Object.keys(pairs).forEach(function(fromLangCode) {
        const toLangCode = pairs[fromLangCode];
        const text = formatLanguageCode(fromLangCode) + ' \u2192 ' +
            formatLanguageCode(toLangCode);

        const li = createLIWithDismissingButton(text, function() {
          chrome.send(
              'removePrefItem', ['whitelists', fromLangCode, toLangCode]);
        });
        ul.appendChild(li);
      });
    }

    let p = $('prefs-too-often-denied');
    p.classList.toggle(
        'prefs-setting-disabled', !detail['translate_too_often_denied']);
    p.appendChild(createDismissingButton(
        chrome.send.bind(null, 'removePrefItem', ['too_often_denied'])));

    if ('translate_recent_target' in detail) {
      const recentTarget = detail['translate_recent_target'];

      p = $('recent-override');

      p.innerHTML = '';

      appendTextFieldWithButton(p, recentTarget, function(value) {
        chrome.send('setRecentTargetLanguage', [value]);
      });
    }

    p = document.querySelector('#prefs-dump p');
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
    ul.innerHTML = '';
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

    p.innerHTML = '';

    if ('country' in details) {
      const country = details['country'];

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
    } else {
      p.textContent = 'Could not load country info from Variations.';
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

    appendTD(tr, formatDate(new Date(detail['time'])), 'detection-logs-time');
    appendTD(tr, detail['url'], 'detection-logs-url');
    appendTD(
        tr, formatLanguageCode(detail['content_language']),
        'detection-logs-content-language');
    appendTD(
        tr, formatLanguageCode(detail['cld_language']),
        'detection-logs-cld-language');
    appendTD(tr, detail['is_cld_reliable'], 'detection-logs-is-cld-reliable');
    appendTD(tr, detail['has_notranslate'], 'detection-logs-has-notranslate');
    appendTD(
        tr, formatLanguageCode(detail['html_root_language']),
        'detection-logs-html-root-language');
    appendTD(
        tr, formatLanguageCode(detail['adopted_language']),
        'detection-logs-adopted-language');
    appendTD(
        tr, formatLanguageCode(detail['content']), 'detection-logs-content');

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
        tr,
        details['error'] + ': ' + formatTranslateErrorsType(details['error']),
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
        tr, details['should_auto_translate'],
        'init-logs-should-auto-translate');
    appendBooleanTD(tr, details['should_show_ui'], 'init-logs-should-show-ui');

    appendTD(
        tr, details['auto_translate_target'],
        'init-logs-auto-translate-target');
    appendTD(
        tr, details['href_translate_target'],
        'init-logs-href-translate-target');
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

  /**
   * The callback entry point from the browser. This function will be
   * called by the browser.
   *
   * @param {string} message The name of the sent message.
   * @param {Object} details The argument of the sent message.
   */
  function messageHandler(message, details) {
    switch (message) {
      case 'languageDetectionInfoAdded':
        onLanguageDetectionInfoAdded(details);
        break;
      case 'prefsUpdated':
        onPrefsUpdated(details);
        break;
      case 'supportedLanguagesUpdated':
        onSupportedLanguagesUpdated(details);
        break;
      case 'countryUpdated':
        onCountryUpdated(details);
        break;
      case 'translateErrorDetailsAdded':
        onTranslateErrorDetailsAdded(details);
        break;
      case 'translateEventDetailsAdded':
        onTranslateEventDetailsAdded(details);
        break;
      case 'translateInitDetailsAdded':
        onTranslateInitDetailsAdded(details);
        break;
      default:
        console.error('Unknown message:', message);
        break;
    }
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

  return {
    initialize: initialize,
    messageHandler: messageHandler,
  };
});

/**
 * The entry point of the UI.
 */
function main() {
  document.addEventListener(
      'DOMContentLoaded', cr.translateInternals.initialize);
}

main();
})();
