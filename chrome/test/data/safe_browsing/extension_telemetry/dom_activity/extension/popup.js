// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendAction(actionName) {
  chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
    if (tabs.length === 0)
      return;
    chrome.tabs.sendMessage(tabs[0].id, {action: actionName}, (response) => {
      console.log(response);
      if (chrome.runtime.lastError) {
        document.getElementById('status').innerText =
            'Error: ' + chrome.runtime.lastError.message;
      } else {
        document.getElementById('status').innerText =
            response.status || (actionName + ' triggered');
      }
    });
  });
}

// DOM Access (Confidentiality)
document.getElementById('btn-cookie')
    .addEventListener('click', () => sendAction('readCookie'));
document.getElementById('btn-input')
    .addEventListener('click', () => sendAction('readInput'));
document.getElementById('btn-textarea')
    .addEventListener('click', () => sendAction('readTextarea'));

// Script Injection (Integrity via executeScript)
document.getElementById('btn-exec-script-func')
    .addEventListener('click', () => {
      chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
        if (tabs.length === 0)
          return;
        chrome.scripting.executeScript({
          target: {tabId: tabs[0].id},
          func: () => {
            console.log('Injected via popup (func)');
          }
        });
        document.getElementById('status').innerText =
            'executeScript (func) Triggered';
      });
    });

document.getElementById('btn-exec-script-file')
    .addEventListener('click', () => {
      chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
        if (tabs.length === 0)
          return;
        chrome.scripting.executeScript(
            {target: {tabId: tabs[0].id}, files: ['injected.js']});
        document.getElementById('status').innerText =
            'executeScript (file) Triggered';
      });
    });

// DOM-based Injection (Integrity)
document.getElementById('btn-add-script')
    .addEventListener('click', () => sendAction('addScript'));
document.getElementById('btn-mod-script')
    .addEventListener('click', () => sendAction('modifyScript'));
document.getElementById('btn-add-iframe')
    .addEventListener('click', () => sendAction('addIframe'));
document.getElementById('btn-add-form')
    .addEventListener('click', () => sendAction('addForm'));
document.getElementById('btn-form-action')
    .addEventListener('click', () => sendAction('modifyFormAction'));
document.getElementById('btn-add-button-formaction')
    .addEventListener('click', () => sendAction('addButtonFormAction'));
document.getElementById('btn-formaction')
    .addEventListener('click', () => sendAction('modifyFormactionAttr'));
document.getElementById('btn-add-link-js')
    .addEventListener('click', () => sendAction('addLinkJS'));
document.getElementById('btn-href-js')
    .addEventListener('click', () => sendAction('modifyHrefJS'));
document.getElementById('btn-add-href-data')
    .addEventListener('click', () => sendAction('addHrefData'));
