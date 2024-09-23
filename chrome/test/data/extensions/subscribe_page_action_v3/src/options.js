// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Various text messages within the edit dialog.
var assistText = chrome.i18n.getMessage("rss_subscription_feed_url_assist");

// Specifies the index of the item we are editing or -1 if adding new.
var editingIndex = -1;
// Whether we are currently editing the default item.
var editingDefault = false;

function main() {
  document.title = chrome.i18n.getMessage("rss_subscription_edit_dialog_title");

  // Make sure the dialog is not visible.
  document.getElementById('dialogBackground').style.display = "none";

  // Make sure the buttons are disabled to begin with.
  document.getElementById('editReader').disabled = true;
  document.getElementById('removeReader').disabled = true;
  document.getElementById('setDefault').disabled = true;

  if (!storageEnabled) {
    document.getElementById('addReader').disabled = true;
    document.getElementById('readerListbox').disabled = true;
    document.getElementById('alwaysUseDefault').disabled = true;
    document.getElementById('resetList').disabled = true;

    alert(chrome.i18n.getMessage("rss_subscription_no_localstorage"));
    return;
  }

  var feedReaderList;
  if (window.localStorage.readerList) {
    feedReaderList = JSON.parse(window.localStorage.readerList);
  } else {
    feedReaderList = defaultReaderList();
    window.localStorage.readerList = JSON.stringify(feedReaderList);
  }

  // If there is no default, set the first item as default.
  if (isDefaultReader("") && feedReaderList.length > 0)
    window.localStorage.defaultReader = feedReaderList[0].url;

  // Populate the list of readers.
  var readerListbox = document.getElementById('readerListbox');
  while (readerListbox.options.length > 0)
    readerListbox.remove(0);
  for (i = 0; i < feedReaderList.length; ++i) {
    var description = feedReaderList[i].description;
    if (isDefaultReader(feedReaderList[i].url))
      description += " " + chrome.i18n.getMessage("rss_subscription_default");
    readerListbox.add(new Option(description, feedReaderList[i].url));
  }

  // Set up the 'show preview?' checkbox.
  var skipPreview = document.getElementById('alwaysUseDefault');
  skipPreview.checked = window.localStorage.showPreviewPage == "No";
}

function toggleFeedPreview() {
  var alwaysUseDefault = document.getElementById('alwaysUseDefault');
  if (alwaysUseDefault.checked)
    window.localStorage.showPreviewPage = "No";
  else
    delete window.localStorage.showPreviewPage;
}

function setDefault() {
  var readerListbox = document.getElementById('readerListbox');
  var selection = readerListbox.options[readerListbox.selectedIndex];
  window.localStorage.defaultReader = selection.value;

  // Reinititalize the page.
  main();
}

function resetList() {
  if (!confirm(chrome.i18n.getMessage("rss_subscription_reset_list_confirm"))) {
    return;
  }

  delete window.localStorage.readerList;
  delete window.localStorage.defaultReader;
  delete window.localStorage.showPreviewPage;

  // Reinititalize the page.
  main();
}

function onSelectionChanged() {
  var selected = readerListbox.selectedIndex > -1;
  // To edit a reader something must be selected.
  document.getElementById('editReader').disabled = !selected;
  // To set default, the current selection cannot already be default.
  document.getElementById('setDefault').disabled = !selected ||
      isDefaultReader(readerListbox[readerListbox.selectedIndex].value);
  // To remove the selected reader it must not be the last item.
  document.getElementById('removeReader').disabled =
      !selected || readerListbox.options.length < 2;
}

function editReader(index) {
  var readerListbox = document.getElementById('readerListbox');

  if (index == -1) {
    // Adding a new item, make sure the text boxes are empty.
    document.getElementById('urlText').value = "";
    document.getElementById('descriptionText').value = "";
    editingIndex = -1;  // New item.
    editingDefault = true;  // New items become default items.
  } else if (index == 0) {
    // Editing some item, find the current item index and store it.
    editingIndex = readerListbox.selectedIndex;
    var oldOption = readerListbox.options[editingIndex];
    document.getElementById('urlText').value = oldOption.value;
    document.getElementById('descriptionText').value =
        oldOption.text.replace(
            ' ' + chrome.i18n.getMessage("rss_subscription_default"), '');
    editingDefault = isDefaultReader(oldOption.value);
  }

  showDialog();
}

function removeReader() {
  var index = readerListbox.selectedIndex;
  var feedReaderList = JSON.parse(window.localStorage.readerList);
  var reply =
    confirm(chrome.i18n.getMessage("rss_subscription_remove_confirm",
                                   feedReaderList[index].description));
  if (reply) {
    var wasDefault = isDefaultReader(feedReaderList[index].url);
    // Remove the item from the list.
    feedReaderList.splice(index, 1);
    window.localStorage.readerList = JSON.stringify(feedReaderList);

    if (wasDefault)
      window.localStorage.defaultReader = feedReaderList[0].url;
  }

  main();
}

/**
 * Shows the Edit Feed Reader dialog.
 */
function showDialog() {
  document.getElementById('urlAssist').innerText = assistText;
  document.getElementById('save').disabled = true;

  // Show the dialog box.
  document.getElementById('dialogBackground').style.display = "-webkit-box";
}

/**
 * Hides the Edit Feed Reader dialog.
 */
function hideDialog() {
  document.getElementById('dialogBackground').style.display = "none";
}

/**
 * Validates the input in the form (making sure something is entered in both
 * fields and that %s is not missing from the url field.
 */
function validateInput() {
  document.getElementById('statusMsg').innerText = "";

  var description = document.getElementById('descriptionText');
  var url = document.getElementById('urlText');

  var valid = (description.value.length > 0 &&
               url.value.length > 0 &&
               url.value.indexOf("%s") > -1);

  document.getElementById('save').disabled = !valid;
}

/**
 * Handler for saving the values.
 */
function save() {
  // Get the old list.
  var feedReaderList = JSON.parse(window.localStorage.readerList);

  var url = document.getElementById('urlText').value;
  var description = document.getElementById('descriptionText').value;

  if (editingIndex == -1) {
    // Construct a new list.
    var newFeedList = [];

    // Add the new item.
    newFeedList.push({ 'url': url, 'description': description });

    for (i = 0; i < feedReaderList.length; ++i)
      newFeedList.push(feedReaderList[i]);

    feedReaderList = newFeedList;
  } else {
    feedReaderList[editingIndex].url = url;
    feedReaderList[editingIndex].description = description;
  }

  window.localStorage.readerList = JSON.stringify(feedReaderList);

  // Retain item default status, in case the url changed while editing item.
  if (editingDefault)
    window.localStorage.defaultReader = url;

  hideDialog();

  // Reload the values from scratch.
  main();
}

function listen(id, eventType, method) {
  document.getElementById(id).addEventListener(eventType, method);
}

document.addEventListener('DOMContentLoaded', function () {
  // Localize.
  i18nReplace('rss_subscription_options');
  i18nReplaceImpl('addReader', 'rss_subscription_add_reader');
  i18nReplaceImpl('editReader', 'rss_subscription_edit_reader');
  i18nReplaceImpl('removeReader', 'rss_subscription_remove_reader');
  i18nReplaceImpl('setDefault', 'rss_subscription_make_default_reader');
  i18nReplaceImpl('resetList', 'rss_subscription_reset_list');
  i18nReplace('rss_subscription_always_use_default');
  i18nReplaceImpl('dialogHeader', 'rss_subscription_edit_dialog_title');
  i18nReplace('rss_subscription_feed_description');
  i18nReplace('rss_subscription_feed_url');
  i18nReplaceImpl('save', 'rss_subscription_save_button', 'value');
  i18nReplaceImpl('rss_subscription_close_button',
                  'rss_subscription_close_button', 'value');
  // Init event listeners.
  listen('readerListbox', 'change', onSelectionChanged);
  listen('addReader', 'click', function () { editReader(-1); });
  listen('editReader', 'click', function () { editReader(0); });
  listen('removeReader', 'click', removeReader);
  listen('setDefault', 'click', setDefault);
  listen('resetList', 'click', resetList);
  listen('alwaysUseDefault', 'change', toggleFeedPreview);
  listen('descriptionText', 'keyup', validateInput);
  listen('urlText', 'keyup', validateInput);
  listen('save', 'click', save);
  listen('rss_subscription_close_button', 'click', hideDialog);

  // Reload the values from scratch.
  main();
});
