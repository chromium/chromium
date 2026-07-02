// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function saveOptions() {
  const cannedResponse = document.getElementById('cannedResponse').value;
  const wordDelay = Number(document.getElementById('wordDelay').value);
  const finalDelay = Number(document.getElementById('finalDelay').value);
  const failOnStart = document.getElementById('failOnStart').checked;

  await chrome.storage.local.set(
      {cannedResponse, wordDelay, finalDelay, failOnStart});
}

async function restoreOptions() {
  const optionsItems = await chrome.storage.local.get(
      {cannedResponse: '', wordDelay: 0, finalDelay: 0, failOnStart: false});

  document.getElementById('cannedResponse').value = optionsItems.cannedResponse;
  document.getElementById('wordDelay').value = optionsItems.wordDelay;
  document.getElementById('finalDelay').value = optionsItems.finalDelay;
  document.getElementById('failOnStart').checked = optionsItems.failOnStart;
}

document.addEventListener('DOMContentLoaded', restoreOptions);
document.getElementById('cannedResponse')
    .addEventListener('change', saveOptions);
document.getElementById('wordDelay').addEventListener('change', saveOptions);
document.getElementById('finalDelay').addEventListener('change', saveOptions);
document.getElementById('failOnStart').addEventListener('change', saveOptions);
