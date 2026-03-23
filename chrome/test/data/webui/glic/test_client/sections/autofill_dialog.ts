// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectAutofillSuggestionsDialogRequest} from '/glic/glic_api/glic_api.js';

import {client, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

let currentRequest: SelectAutofillSuggestionsDialogRequest|null = null;

function clearUi() {
  $.autofillSuggestionsDialogStatus.textContent = 'Idle';
  $.autofillSuggestionsList.innerHTML = '';
  $.selectedAutofillSuggestionId.value = '';
  $.autofillSuggestionsDialogSection.hidden = true;
}

function handleRequest(request: SelectAutofillSuggestionsDialogRequest) {
  currentRequest = request;
  $.autofillSuggestionsDialogSection.hidden = false;
  $.autofillSuggestionsDialogStatus.textContent =
      'Task requests suggestion selection.';

  request.formFillingRequests.forEach((formFillingRequest, formIndex) => {
    const li_form = document.createElement('li');
    li_form.textContent =
        `Form for requestedData: ${formFillingRequest.requestedData}`;

    const originContainer = document.createElement('div');
    originContainer.appendChild(document.createTextNode('Origin: '));
    const originValue = document.createElement('span');
    originValue.id = `formatted-request-origin-${formIndex}`;
    originValue.textContent = formFillingRequest.formattedRequestOrigin ?? null;
    originContainer.appendChild(originValue);
    li_form.appendChild(originContainer);

    const notifyFormPresentedBtn = document.createElement('button');
    notifyFormPresentedBtn.textContent = 'Notify Form Presented';
    notifyFormPresentedBtn.id = `notify-form-presented-${formIndex}`;
    notifyFormPresentedBtn.addEventListener('click', () => {
      request.onFormPresented?.({formFillingRequestIndex: formIndex});
      logMessage(
          `Notified form presented for formFillingRequestIndex ${formIndex}`);
    });
    li_form.appendChild(notifyFormPresentedBtn);

    const suggestionActionsContainer = document.createElement('div');

    const suggestionInput = document.createElement('input');
    suggestionInput.type = 'text';
    suggestionInput.id = `suggestion-input-${formIndex}`;
    if (formFillingRequest.suggestions.length > 0) {
      suggestionInput.value = formFillingRequest.suggestions[0]!.id;
    }
    suggestionActionsContainer.appendChild(suggestionInput);

    const previewInput = document.createElement('button');
    previewInput.textContent = 'Preview';
    previewInput.id = `preview-input-btn-${formIndex}`;
    previewInput.addEventListener('click', () => {
      if (suggestionInput.value !== '') {
        request.onFormPreviewChanged?.({
          formFillingRequestIndex: formIndex,
          response: {selectedSuggestionId: suggestionInput.value},
        });
      } else {
        request.onFormPreviewChanged?.({
          formFillingRequestIndex: formIndex,
        });
      }
      logMessage(`Preview started for formFillingRequestIndex ${
          formIndex}, id ${suggestionInput.value}`);
    });
    suggestionActionsContainer.appendChild(previewInput);

    const confirmFormBtn = document.createElement('button');
    confirmFormBtn.textContent = 'Confirm';
    confirmFormBtn.id = `confirm-form-btn-${formIndex}`;
    confirmFormBtn.addEventListener('click', () => {
      request.onFormConfirmed?.({
        formFillingRequestIndex: formIndex,
        response: {selectedSuggestionId: suggestionInput.value},
      });
      logMessage(`Form confirmed for formFillingRequestIndex ${formIndex}, id ${
          suggestionInput.value}`);
    });
    suggestionActionsContainer.appendChild(confirmFormBtn);

    li_form.appendChild(suggestionActionsContainer);

    const ul_suggestions = document.createElement('ul');
    for (const suggestion of formFillingRequest.suggestions) {
      const li_suggestion = document.createElement('li');
      li_suggestion.textContent = `ID: ${suggestion.id}, Title: ${
          suggestion.title}, Details: ${suggestion.details}`;

      const previewStartBtn = document.createElement('button');
      previewStartBtn.textContent = 'Preview Start';
      previewStartBtn.addEventListener('click', () => {
        request.onFormPreviewChanged?.({
          formFillingRequestIndex: formIndex,
          response: {selectedSuggestionId: suggestion.id},
        });
        logMessage(`Preview started for formFillingRequestIndex ${
            formIndex}, id ${suggestion.id}`);
      });
      li_suggestion.appendChild(previewStartBtn);

      const previewEndBtn = document.createElement('button');
      previewEndBtn.textContent = 'Preview End';
      previewEndBtn.addEventListener('click', () => {
        request.onFormPreviewChanged?.({formFillingRequestIndex: formIndex});
        logMessage(`Preview ended for formFillingRequestIndex ${formIndex}`);
      });
      li_suggestion.appendChild(previewEndBtn);

      ul_suggestions.appendChild(li_suggestion);
    }
    li_form.appendChild(ul_suggestions);

    const confirmBtn = document.createElement('button');
    confirmBtn.textContent = 'Confirm Form';
    confirmBtn.addEventListener('click', () => {
      // Use the first suggestion as the default "confirmation" for this form.
      if (formFillingRequest.suggestions.length > 0) {
        const id = formFillingRequest.suggestions[0]!.id;
        request.onFormConfirmed?.({
          formFillingRequestIndex: formIndex,
          response: {selectedSuggestionId: id},
        });
        logMessage(`Form confirmed for formFillingRequestIndex ${
            formIndex}, id ${id}`);
      }
    });
    li_form.appendChild(confirmBtn);

    $.autofillSuggestionsList.appendChild(li_form);
  });

  if (request.formFillingRequests.length > 0) {
    // Pre-fill the selection with the first suggestion from each form filling
    // request.
    $.selectedAutofillSuggestionId.value =
        String(request.formFillingRequests
                   .map((ffReq) => ffReq.suggestions[0]?.id || '')
                   .reduce((a, b) => a + (a === '' ? '' : ',') + b, ''));
  }
}

$.sendAutofillSuggestionsResponse.addEventListener('click', () => {
  if (!currentRequest) {
    return;
  }
  const selectedIds = ($.selectedAutofillSuggestionId.value || '').split(',');
  currentRequest.onDialogClosed({
    response: {
      selectedSuggestions: selectedIds.map((id) => {
        return {
          selectedSuggestionId: id,
        };
      }),
    },
  });
  logMessage('Sent autofill suggestion response.');
  clearUi();
  currentRequest = null;
});

$.cancelAutofillSuggestionsDialog.addEventListener('click', () => {
  if (!currentRequest) {
    return;
  }
  currentRequest.onDialogClosed({
    response: {
      selectedSuggestions: [],
    },
  });
  logMessage('Sent empty selection suggestion response.');
  clearUi();
  currentRequest = null;
});


client.getInitialized().then(() => {
  const statusDiv = document.createElement('div');
  statusDiv.id = 'autofill-setup-status';
  document.body.appendChild(statusDiv);
  const handler =
      client.browser?.selectAutofillSuggestionsDialogRequestHandler?.();
  if (handler) {
    statusDiv.textContent = 'handler-found';
    handler.subscribe(handleRequest);
    clearUi();
  } else {
    statusDiv.textContent = 'handler-NOT-found';
    $.autofillSuggestionsDialogSection.hidden = true;
    logMessage('Autofill suggestion dialog handler not available.');
  }
});
