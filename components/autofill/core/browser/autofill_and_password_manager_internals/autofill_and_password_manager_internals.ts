// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

// By default this page only records metrics for a given period of time in order
// to not waste too much memory. This constant defines the default period until
// recording ceases.
const kDefaultLoggingPeriodInSeconds: number = 300;

// Indicates whether logs should be recorded at the moment.
let recordLogs: boolean = true;

// Renders a simple dialog with |text| as a message and a close button.
function showModalDialog(text: string) {
  const dialog = document.createElement('div');
  dialog.className = 'modal-dialog';

  const content = document.createElement('div');
  content.className = 'modal-dialog-content';

  const closeButton = document.createElement('span');
  closeButton.className = 'modal-dialog-close-button fake-button';
  closeButton.innerText = 'Close';

  const textContent = document.createElement('p');
  textContent.className = 'modal-dialog-text';
  textContent.innerText = text;

  content.appendChild(closeButton);
  content.appendChild(textContent);
  dialog.appendChild(content);
  window.document.body.append(dialog);

  closeButton.addEventListener('click', () => {
    window.document.body.removeChild(dialog);
  });
}

// Autoscrolling keeps the page scrolled down. Intended usage is as follows:
// before modifying the DOM, check needsScrollDown(), and afterwards invoke
// scrollDown() if needsScrollDown() was true.

function isScrolledDown(): boolean {
  return window.innerHeight + window.scrollY >= document.body.offsetHeight;
}

// Whether autoscroll is currently scrolling.
let autoScrollActive: boolean = false;
let autoScrollTimer: number = 0;  // Timer for resetting |autoScrollActive|.

function needsScrollDown(): boolean {
  const checkbox =
      document.querySelector<HTMLInputElement>('#EnableAutoscroll');
  return autoScrollActive || (isScrolledDown() && !!checkbox?.checked);
}

function scrollDown() {
  autoScrollActive = true;
  window.scrollTo(0, document.body.scrollHeight);
  (function unsetAutoScrollActiveAfterIdletime() {
    if (isScrolledDown()) {
      autoScrollActive = false;
    } else {
      clearTimeout(autoScrollTimer);
      autoScrollTimer = setTimeout(unsetAutoScrollActiveAfterIdletime, 50);
    }
  })();
}

// The configuration of log display can be represented in the URI fragment.
// Below are utility functions for setting/getting these parameters.

function makeKeyValueRegExp(key: string): RegExp {
  return new RegExp(`\\b${key}=([^&]*)`);
}

function setUrlHashParam(key: string, value: string) {
  key = encodeURIComponent(key);
  value = encodeURIComponent(value);
  const keyValueRegExp = makeKeyValueRegExp(key);
  const keyValue = `${key}=${value}`;
  if (keyValueRegExp.test(window.location.hash)) {
    const replaced = window.location.hash.replace(keyValueRegExp, keyValue);
    window.location.hash = replaced;
  } else {
    window.location.hash +=
        (window.location.hash.length > 0 ? '&' : '') + keyValue;
  }
}

function getUrlHashParam(key: string): string|undefined {
  key = encodeURIComponent(key);
  const match = window.location.hash.match(makeKeyValueRegExp(key));
  if (!match || match[1] === undefined) {
    return undefined;
  }
  return decodeURIComponent(match[1]);
}

interface InternalNode {
  type: 'element'|'text'|'fragment';
  value: string;
  children?: InternalNode[];
  attributes?: {[key: string]: string};
}

// Converts an internal representation of nodes to actual DOM nodes that can
// be attached to the DOM. The internal representation.
// If a node contains PII data, all its children texts are stripped, unless it
// is explicit set by the user that PII values can be displayed.
function nodeToDomNode(node: InternalNode, parentContainsPII = false): Node {
  if (node.type === 'text') {
    const displayPIIEnabled =
        getRequiredElement<HTMLInputElement>('DisplayPii').checked;
    const canDisplayNodeValue = !parentContainsPII || displayPIIEnabled;
    return document.createTextNode(
        canDisplayNodeValue ? node.value : 'PII stripped');
  }
  // Else the node is of type 'element'.
  const domNode = document.createElement(node.value);
  if (node.attributes) {
    for (const [attribute, value] of Object.entries(node.attributes)) {
      domNode.setAttribute(attribute, value);
    }
  }
  if (node.children) {
    const updatedParentContainsPII = parentContainsPII ||
        (node.attributes && node.attributes['data-pii'] === 'true');
    node.children.forEach(child => {
      domNode.appendChild(nodeToDomNode(child, updatedParentContainsPII));
    });
  }
  return domNode;
}

function addStructuredLog(
    node: InternalNode, ignoreRecordLogs: boolean = false) {
  if (!recordLogs && !ignoreRecordLogs) {
    return;
  }
  const logDiv = getRequiredElement('log-entries');
  if (!logDiv) {
    return;
  }
  const scrollAfterInsert = needsScrollDown();
  logDiv.appendChild(document.createElement('hr'));
  if (node.type === 'fragment') {
    if (node.children) {
      node.children.forEach(child => {
        logDiv.appendChild(nodeToDomNode(child));
      });
    }
  } else {
    logDiv.appendChild(nodeToDomNode(node));
  }
  if (scrollAfterInsert) {
    scrollDown();
  }
}

// Sets up a couple of event handlers and interval handlers for automatically
// stopping the recording of autofill events. We stop the recording because
// you may forget an internals page in some tab and don't want it to keep
// growing it's memory consumption forever.
// We have two checkboxes
// [x] Record new events
// [x] Automatically stop recording in 0:30
// with the following behavior:
// - While the first checkbox is checked, log entries are recorded.
// - While both checkboxes are checked, the countdown decreases.
// - If the countdown reaches 0:00, the first checkbox gets unchecked.
// - If any checkbox is toggled, we reset the countdown time to
//   kDefaultLoggingPeriodInSeconds.
function setUpStopRecording() {
  // Timestamp (in ms after epoch), when the countdown to stop recording should
  // happen.
  let stopRecordingLogsAt: number|undefined;
  // Interval ID generated by setInterval, which is called every second to
  // update the remaining time.
  let countdown: number|undefined;

  const currentlyRecordingChkBox =
      getRequiredElement<HTMLInputElement>('CurrentlyRecording');
  const autoStopRecordingChkBox =
      getRequiredElement<HTMLInputElement>('AutomaticallyStopRecording');

  // Formats a number of seconds into a [M]M:SS format.
  const secondsToString = (seconds: number) => {
    const minutes = Math.floor(seconds / 60);
    seconds = seconds % 60;
    return `${minutes}:${seconds < 10 ? '0' : ''}${seconds}`;
  };

  // Updates the time label and reacts to the countdown reaching 0.
  const countdownHandler = () => {
    assert(stopRecordingLogsAt);
    const remainingSeconds = Math.round(
        Math.max((stopRecordingLogsAt - new Date().getTime()) / 1000, 0));
    getRequiredElement('stop-recording-time').innerText =
        secondsToString(remainingSeconds);

    if (remainingSeconds === 0) {
      recordLogs = false;
      currentlyRecordingChkBox.checked = false;
      resetTimeout();
    }
  };
  const startCountDown = () => {
    if (!countdown) {
      countdown = window.setInterval(countdownHandler, 1000);
    }
  };
  const stopCountDown = () => {
    if (countdown) {
      window.clearInterval(countdown);
      countdown = undefined;
    }
  };
  const startOrStopCountDown = () => {
    if (currentlyRecordingChkBox.checked && autoStopRecordingChkBox.checked) {
      startCountDown();
    } else {
      stopCountDown();
    }
  };
  const resetTimeout = () => {
    stopRecordingLogsAt =
        new Date().getTime() + kDefaultLoggingPeriodInSeconds * 1000;
    countdownHandler();  // Update the string shown to the user.
    startOrStopCountDown();
  };

  currentlyRecordingChkBox.addEventListener('click', () => {
    recordLogs = currentlyRecordingChkBox.checked;
    resetTimeout();
  });
  autoStopRecordingChkBox.addEventListener('click', () => {
    resetTimeout();
  });

  resetTimeout();
}

function setUpAutofillInternals(autofillAiEnabled: boolean) {
  document.title = 'Autofill Internals';
  getRequiredElement('h1-title').textContent = 'Autofill Internals';
  getRequiredElement('logging-note').innerText =
      'Captured autofill logs are listed below. Logs are cleared and no longer \
      captured when all autofill-internals pages are closed.';
  getRequiredElement('logging-note-incognito').innerText =
      'Captured autofill logs are not available in Incognito.';
  setUpScopeCheckboxes();
  setUpSettingCheckboxe();
  setUpMarker();
  setUpSubmittedFormsJSONDataDownload();
  setUpButtonForDomNodeIdCapture();
  setUpDownload('autofill');
  if (autofillAiEnabled) {
    addAutofillTabs();
  }
  setUpStopRecording();
}

function setUpPasswordManagerInternals() {
  document.title = 'Password Manager Internals';
  getRequiredElement('h1-title').textContent = 'Password Manager Internals';
  getRequiredElement('logging-note').innerText =
      'Captured password manager logs are listed below. Logs are cleared and \
      no longer captured when all password-manager-internals pages are closed.';
  getRequiredElement('logging-note-incognito').innerText =
      'Captured password manager logs are not available in Incognito.';
  setUpSettingCheckboxe();
  setUpMarker();
  setUpDownload('password-manager');
  setUpStopRecording();
  // <if expr="is_android">
  getRequiredElement('reset-upm-eviction-fake-button').style.display = 'inline';
  addWebUiListener(
      'enable-reset-upm-eviction-button', enableResetUpmEvictionButton);
  // </if>
}

function enableResetCacheButton() {
  getRequiredElement('reset-cache-fake-button').style.display = 'inline';
}

// <if expr="is_android">
function enableResetUpmEvictionButton(isEnabled: boolean) {
  getRequiredElement('reset-upm-eviction-fake-button').innerText =
      isEnabled ? 'Reset UPM eviction' : 'Evict from UPM';
}
// </if>

function notifyAboutIncognito(isIncognito: boolean) {
  document.body.dataset['incognito'] = isIncognito.toString();
}

function notifyAboutVariations(variations: string[]) {
  const list = document.createElement('div');
  for (const item of variations) {
    list.appendChild(document.createTextNode(item));
    list.appendChild(document.createElement('br'));
  }
  const variationsList =
      getRequiredElement<HTMLTableCellElement>('variations-list');
  variationsList.appendChild(list);
}

// Setup a (fake) button to add visual markers
// (it's fake to keep Autofill from parsing the form).
function setUpMarker() {
  // Initialize marker field: when pressed, add fake log event.
  let markerCounter = 0;
  const markerFakeButton =
      getRequiredElement<HTMLButtonElement>('marker-fake-button');
  markerFakeButton.addEventListener('click', () => {
    ++markerCounter;
    const scrollAfterInsert = needsScrollDown();
    addStructuredLog(
        {
          type: 'element',
          value: 'div',
          attributes: {'class': 'marker', 'contenteditable': 'true'},
          children: [{type: 'text', value: `#${markerCounter} `}],
        },
        /*ignoreRecordLogs=*/ true);
    if (scrollAfterInsert) {
      scrollDown();
      // Focus marker div, set caret at end of line.
      const logDiv = getRequiredElement('log-entries');
      const markerNode = logDiv.lastChild as HTMLElement;
      const textNode = markerNode.lastChild as Text;
      markerNode.focus();
      window.getSelection()!.collapse(textNode, textNode.length);
    }
  });
}

// Setup a (fake) download button to download html content of the page.
function setUpDownload(moduleName: string) {
  const downloadFakeButton = getRequiredElement('download-fake-button');
  downloadFakeButton.addEventListener('click', () => {
    const html = document.documentElement.outerHTML;
    const blob = new Blob([html], {type: 'text/html'});
    const url = window.URL.createObjectURL(blob);
    const dateString = new Date()
                           .toISOString()
                           .replace(/T/g, '_')
                           .replace(/\..+/, '')
                           .replace(/:/g, '-');
    const filename = `${moduleName}-internals-${dateString}.html`;

    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    a.remove();
  });
  // <if expr="is_ios">
  // Hide this until downloading a file works on iOS, see
  // https://bugs.webkit.org/show_bug.cgi?id=167341
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1252380
  downloadFakeButton.style.display = 'none';
  // </if>
}

interface SubmittedFormTopLevelData {
  timestamp: string;
  'Renderer id:'?: string;
  'URL:'?: string;
}

// Retrieve the top level data about a submitted form:
// 1. Timestamp
// 2. Renderer id
// 3. URL
//
// Note that a form is not a html <form /> tag, but a <div> whose
// scope attribute is "Submission". Such a div contains children information
// related to a submitted form detected by Autofill.
function getSubmittedFormTopLevelData(form: HTMLElement):
    SubmittedFormTopLevelData {
  const formTopLevelData: Record<string, string> = {};
  const formLevelDataOfInterest = new Set(['Renderer id:', 'URL:']);
  const childrenTableElements: HTMLCollectionOf<HTMLTableCellElement> =
      form.getElementsByTagName('td');
  for (const childTableElement of childrenTableElements) {
    if (!formLevelDataOfInterest.has(childTableElement.innerText)) {
      continue;
    }

    formTopLevelData[childTableElement.innerText] =
        (childTableElement.nextSibling! as HTMLElement).innerText;
    // If all interested top level entries were found, we can early return.
    if (Object.keys(formTopLevelData).length === formLevelDataOfInterest.size) {
      break;
    }
  }

  // Include the submission timestamp information.
  const getSubmissionTimestamp = (): string => {
    // Find the substring "timestamp: 123456789";
    const timestampSection = form.textContent!.match(/timestamp: ([0-9]+)/);
    return timestampSection ? timestampSection[1]! : 'Not found';
  };

  return {timestamp: getSubmissionTimestamp(), ...formTopLevelData};
}

interface SubmittedFormFieldsData {
  [key: string]: string;
}

// Retrieve the field level data about the submitted form.
function getSubmittedFormFieldsData(form: HTMLElement):
    SubmittedFormFieldsData[] {
  // The children are organized inside <td> tags.
  const childrenTableElements = form.getElementsByTagName('td');
  // Regex to match "Field: " strings.
  const fieldRegexPattern = /Field\s[0-9]+:/;
  // As of the time of writing this CL, only labels and values are interesting
  // to us.
  const fieldsOfInterest = new Set(['Label:', 'Value:']);

  const fieldsData = [];
  for (const childTableElement of childrenTableElements) {
    if (!fieldRegexPattern.test(childTableElement.innerText)) {
      continue;
    }

    // The next sibling contains the actual data name and value we are
    // interested in.
    //  <td> Field 1:</td> <- Matched by the regex above.
    //  <td> <- Next sibling
    //    <table>
    //      </table>
    //        <tr> <- children containing the information we want.
    //          <td>Label: </td>
    //          <td>First name</td>
    //        </tr>
    //      </table>
    //    </table>
    //  </td>
    const elementRows =
        childTableElement.nextElementSibling!.querySelectorAll('tr');
    const fieldData: Record<string, string> = {};
    for (const row of elementRows) {
      // It is expected two children, in the example above that would be:
      // <td>Label: </td>
      // <td>First name</td>
      if (row.children.length !== 2) {
        continue;
      }

      let name = (row.children[0] as HTMLElement).innerText;
      if (!fieldsOfInterest.has(name)) {
        continue;
      }

      // Remove trailing ":"
      name = name.substring(0, name.length - 1);
      const value = (row.children[1] as HTMLElement).innerText;
      fieldData[name] = value;
    }
    fieldsData.push(fieldData);
  }
  return fieldsData;
}

interface SubmittedFormData extends SubmittedFormTopLevelData {
  fields: SubmittedFormFieldsData[];
}

function getSubmittedFormData(form: HTMLElement): SubmittedFormData {
  const formData = getSubmittedFormTopLevelData(form);
  const formFieldsData = getSubmittedFormFieldsData(form);
  return {
    ...formData,
    fields: formFieldsData,
  };
}

// Setup a (fake) download button to download a json file containing information
// about the submitted forms.
function setUpSubmittedFormsJSONDataDownload() {
  const downloadSubmittedFormJSONDataButton =
      getRequiredElement('download-submitted-forms-json-data-fake-button');
  downloadSubmittedFormJSONDataButton.style.display = 'inline';
  downloadSubmittedFormJSONDataButton.addEventListener('click', () => {
    const formsSubmittedSection =
        document.querySelectorAll<HTMLElement>('[scope="Submission"]');
    const parsedFormData = [...formsSubmittedSection].map(
        submittedForm => getSubmittedFormData(submittedForm));
    const dataStr = 'data:application/json;charset=utf-8,' +
        encodeURIComponent(JSON.stringify(parsedFormData, null, 2));
    const a = document.createElement('a');
    a.href = dataStr;
    const dateString = new Date()
                           .toISOString()
                           .replace(/T/g, '_')
                           .replace(/\..+/, '')
                           .replace(/:/g, '-');
    const filename = `autofill-internals-submitted-forms-${dateString}.json`;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
  });
  // <if expr="is_ios">
  // Hide this until downloading a file works on iOS, see
  // https://bugs.webkit.org/show_bug.cgi?id=167341
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1252380
  downloadSubmittedFormJSONDataButton.style.display = 'none';
  // </if>
}

function setUpButtonForDomNodeIdCapture() {
  // <if expr="not is_android and not is_ios" >
  const button = document.getElementById('set-dom-node-id')!;
  button.style.display = 'inline';
  button.addEventListener('click', () => {
    chrome.send('setDomNodeId');
  });
  // </if>
}

interface CheckboxInfo {
  id: string;
  label?: string;
  uncheckedByDefault?: boolean;
}

// Creates a checkbox for a given JSON struct `info`. Given
//   {id: "Foo", label: "Bar", uncheckedByDefault: someBool }
// this creates
//   <label><input type=checkbox id="Foo"> Bar</label>
// and returns the <input> element.
//
// Whether the checkbox is checked depends on the current URL's hash param or,
// as a fallback, `info.uncheckedByDefault`.
//
// `info.id` is mandatory.
// `info.label` defaults to `info.id`.
// `info.uncheckedByDefault` defaults to false.
function createCheckbox(info: CheckboxInfo): HTMLInputElement {
  const input = document.createElement('input');
  input.setAttribute('type', 'checkbox');
  input.setAttribute('id', info.id);
  input.checked = info.uncheckedByDefault ? getUrlHashParam(info.id) === 'y' :
                                            getUrlHashParam(info.id) !== 'n';
  input.addEventListener('change', () => {
    setUrlHashParam(info.id, input.checked ? 'y' : 'n');
  });
  const label = document.createElement('label');
  label.appendChild(input);
  label.appendChild(document.createTextNode(' ' + (info.label || info.id)));
  return input;
}

// Sets up the top bar with checkboxes to show/hide the different sorts of log
// event types.
function setUpScopeCheckboxes() {
  const logDiv = getRequiredElement('log-entries');
  const scopesPlaceholder = getRequiredElement('scopes-checkbox-placeholder');

  // Create and initialize filter checkboxes: remove/add hide-<Scope> class to
  // |logDiv| when (un)checked.
  const SCOPES: CheckboxInfo[] = [
    {id: 'Context'},
    {id: 'Parsing'},
    {id: 'AbortParsing'},
    {id: 'Filling'},
    {id: 'Submission'},
    {id: 'AutofillServer'},
    {id: 'Metrics'},
    {id: 'AddressProfileFormImport'},
    {id: 'WebsiteModifiedFieldValue'},
    {id: 'FastCheckout', uncheckedByDefault: true},
    {id: 'TouchToFill'},
    {id: 'AutofillAi'},
  ];
  for (const scope of SCOPES) {
    const input = createCheckbox(scope);
    scopesPlaceholder.appendChild(input.parentElement!);
    function changeHandler() {
      const cls = `hide-${scope.id}`;
      const scrollAfterInsert = needsScrollDown();
      if (!input.checked) {
        logDiv.classList.add(cls);
      } else {
        logDiv.classList.remove(cls);
      }
      if (scrollAfterInsert) {
        scrollDown();
      }
    }
    input.addEventListener('change', changeHandler);
    changeHandler();  // Call once to initialize |logDiv|'s classes.
  }
}

// Sets up another bar of checkboxes to configure the page's behavior.
function setUpSettingCheckboxe() {
  const settingsPlaceholder =
      getRequiredElement('settings-checkbox-placeholder');

  // Create and initialize the settings checkboxes.
  const SETTINGS: CheckboxInfo[] = [
    {id: 'EnableAutoscroll', label: 'Scroll down'},
    {id: 'CurrentlyRecording', label: 'Record new events'},
    {id: 'AutomaticallyStopRecording', label: 'Stop recording in '},
    {id: 'DisplayPii', label: 'Display PII', uncheckedByDefault: true},
  ];
  for (const setting of SETTINGS) {
    const input = createCheckbox(setting);
    settingsPlaceholder.appendChild(input.parentElement!);
  }
  {
    // Add the timestamp for AutomaticallyStopRecording.
    const span = document.createElement('span');
    span.id = 'stop-recording-time';
    span.innerText = 'M:SS';
    getRequiredElement('AutomaticallyStopRecording')
        .parentElement!.appendChild(span);
  }
}

function addTabLink(linkText: string, tabId: string) {
  const tabsDiv = getRequiredElement('tab-links');
  const link = document.createElement('a');
  link.innerText = linkText;
  link.addEventListener('click', () => {
    const tabsContainer = getRequiredElement('tabs-container');
    for (const tab of tabsContainer.children) {
      if (tab instanceof HTMLElement) {
        tab.style.display = 'none';
      }
    }
    getRequiredElement(tabId).style.display = 'block';
    onTabShown(tabId);
  });
  tabsDiv.appendChild(link);
}

function onTabShown(tabId: string) {
  if (tabId === 'tab-autofill-ai-cache') {
    chrome.send('getAutofillAiCache');
  }
}

function addAutofillTabs() {
  addTabLink('Autofill logs', 'tab-logs');
  addTabLink('AutofillAI cache', 'tab-autofill-ai-cache');
  getRequiredElement('tab-links').style.display = 'block';
}

interface AutofillAiFieldCacheEntry {
  signature: string;
  rank: string;
  type: string;
  format?: string;
}

interface AutofillAiCacheEntry {
  formSignature: string;
  creationTime: string;
  fields: AutofillAiFieldCacheEntry[];
}

function displayAutofillAiCache(entries: AutofillAiCacheEntry[]) {
  const container = getRequiredElement('tab-autofill-ai-cache');
  if (entries.length === 0) {
    container.innerText = 'Cache is empty.';
    return;
  }

  container.innerText = '';
  for (const entry of entries) {
    const entryTable = document.createElement('table');
    entryTable.className = 'cache-entry';
    const entryHeader = document.createElement('th');

    entryHeader.innerText = `Form signature: ${
        entry.formSignature}, creation time: ${entry.creationTime}.`;
    const deleteButton = document.createElement('span');
    deleteButton.innerText = 'Remove';
    deleteButton.className = 'fake-button delete-cache-entry-button';
    deleteButton.addEventListener('click', () => {
      chrome.send('removeAutofillAiCacheEntry', [entry.formSignature]);
      chrome.send('getAutofillAiCache');
    });
    entryHeader.appendChild(deleteButton);
    entryTable.appendChild(entryHeader);

    for (const field of entry.fields) {
      const row = document.createElement('tr');
      row.innerText = `Signature = ${field.signature}, rank = ${
          field.rank}, type = ${field.type}`;
      if (field.format) {
        row.innerText += `, format = ${field.format}`;
      }
      entryTable.appendChild(row);
    }
    container.appendChild(entryTable);
    container.appendChild(document.createElement('hr'));
  }
}

document.addEventListener('DOMContentLoaded', () => {
  addWebUiListener('enable-reset-cache-button', enableResetCacheButton);
  addWebUiListener('notify-about-incognito', notifyAboutIncognito);
  addWebUiListener('notify-about-variations', notifyAboutVariations);
  addWebUiListener(
      'notify-reset-done', (message: string) => showModalDialog(message));
  addWebUiListener('add-structured-log', addStructuredLog);
  addWebUiListener('display-autofill-ai-cache', displayAutofillAiCache);
  addWebUiListener('setup-autofill-internals', setUpAutofillInternals);
  addWebUiListener(
      'setup-password-manager-internals', setUpPasswordManagerInternals);

  chrome.send('loaded');

  const resetCacheFakeButton = getRequiredElement('reset-cache-fake-button');
  resetCacheFakeButton.addEventListener('click', () => {
    chrome.send('resetCache');
  });

  const resetUpmEvictionButton =
      getRequiredElement('reset-upm-eviction-fake-button');
  resetUpmEvictionButton.addEventListener('click', () => {
    chrome.send('resetUpmEviction');
  });
});
