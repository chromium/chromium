// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function pointInElement(p, elem) {
  return ((p.x >= elem.offsetLeft) &&
          (p.x <= (elem.offsetLeft + elem.offsetWidth)) &&
          (p.y >= elem.offsetTop) &&
          (p.y <= (elem.offsetTop + elem.offsetHeight)));
};

function setLastOpened() {
  localStorage.popupLastOpened = (new Date()).getTime();
  chrome.runtime.sendMessage('poll');
};

function loadI18nMessages() {
  function setProperty(selector, prop, msg) {
    document.querySelector(selector)[prop] = chrome.i18n.getMessage(msg);
  }

  setProperty('title', 'innerText', 'tabTitle');
  setProperty('#q', 'placeholder', 'searchPlaceholder');
  setProperty('#clear-all', 'title', 'clearAllTitle');
  setProperty('#open-folder', 'title', 'openDownloadsFolderTitle');
  setProperty('#empty', 'innerText', 'zeroItems');
  setProperty('#searching', 'innerText', 'searching');
  setProperty('#search-zero', 'innerText', 'zeroSearchResults');
  setProperty('#management-permission-info', 'innerText',
              'managementPermissionInfo');
  setProperty('#grant-management-permission', 'innerText',
              'grantManagementPermission');
  setProperty('#older', 'innerText', 'showOlderDownloads');
  setProperty('#loading-older', 'innerText', 'loadingOlderDownloads');
  setProperty('.pause', 'title', 'pauseTitle');
  setProperty('.resume', 'title', 'resumeTitle');
  setProperty('.cancel', 'title', 'cancelTitle');
  setProperty('.show-folder', 'title', 'showInFolderTitle');
  setProperty('.erase', 'title', 'eraseTitle');
  setProperty('.url', 'title', 'retryTitle');
  setProperty('.referrer', 'title', 'referrerTitle');
  setProperty('.open-filename', 'title', 'openTitle');
  setProperty('#bad-chrome-version', 'innerText', 'badChromeVersion');
  setProperty('.remove-file', 'title', 'removeFileTitle');

  document.querySelector('.progress').style.minWidth =
    getTextWidth(formatBytes(1024 * 1024 * 1023.9) + '/' +
                 formatBytes(1024 * 1024 * 1023.9)) + 'px';

  // This only covers {timeLeft,openWhenComplete}{Finishing,Days}. If
  // ...Hours/Minutes/Seconds could be longer for any locale, then this should
  // test them.
  var max_time_left_width = 0;
  for (var i = 0; i < 4; ++i) {
    max_time_left_width = Math.max(max_time_left_width, getTextWidth(
        formatTimeLeft(0 == (i % 2),
                       (i < 2) ? 0 : ((100 * 24) + 23) * 60 * 60 * 1000)));
  }
  document.querySelector('body div.item span.time-left').style.minWidth =
     max_time_left_width + 'px';
};

function getTextWidth(s) {
  var probe = document.getElementById('text-width-probe');
  probe.innerText = s;
  return probe.offsetWidth;
};

function formatDateTime(date) {
  var now = new Date();
  var zpad_mins = ':' + (date.getMinutes() < 10 ? '0' : '') + date.getMinutes();
  if (date.getYear() != now.getYear()) {
    return '' + (1900 + date.getYear());
  } else if ((date.getMonth() != now.getMonth()) ||
             (date.getDate() != now.getDate())) {
    return date.getDate() + ' ' + chrome.i18n.getMessage(
      'month' + date.getMonth() + 'abbr');
  } else if (date.getHours() == 12) {
    return '12' + zpad_mins + 'pm';
  } else if (date.getHours() > 12) {
    return (date.getHours() - 12) + zpad_mins + 'pm';
  }
  return date.getHours() + zpad_mins + 'am';
}

function formatBytes(n) {
  if (n < 1024) {
    return n + 'B';
  }
  var prefixes = 'KMGTPEZY';
  var mul = 1024;
  for (var i = 0; i < prefixes.length; ++i) {
    if (n < (1024 * mul)) {
      return (parseInt(n / mul) + '.' + parseInt(10 * ((n / mul) % 1)) +
              prefixes[i] + 'B');
    }
    mul *= 1024;
  }
  return '!!!';
}

function formatTimeLeft(openWhenComplete, ms) {
  var prefix = openWhenComplete ? 'openWhenComplete' : 'timeLeft';
  if (ms < 1000) {
    return chrome.i18n.getMessage(prefix + 'Finishing');
  }
  var days = parseInt(ms / (24 * 60 * 60 * 1000));
  var hours = parseInt(ms / (60 * 60 * 1000)) % 24;
  if (days) {
    return chrome.i18n.getMessage(prefix + 'Days', [days, hours]);
  }
  var minutes = parseInt(ms / (60 * 1000)) % 60;
  if (hours) {
    return chrome.i18n.getMessage(prefix + 'Hours', [hours, minutes]);
  }
  var seconds = parseInt(ms / 1000) % 60;
  if (minutes) {
    return chrome.i18n.getMessage(prefix + 'Minutes', [minutes, seconds]);
  }
  return chrome.i18n.getMessage(prefix + 'Seconds', [seconds]);
}

function ratchetWidth(w) {
  var current = parseInt(document.body.style.minWidth) || 0;
  document.body.style.minWidth = Math.max(w, current) + 'px';
}

function ratchetHeight(h) {
  var current = parseInt(document.body.style.minHeight) || 0;
  document.body.style.minHeight = Math.max(h, current) + 'px';
}

function binarySearch(array, target, cmp) {
  var low = 0, high = array.length - 1, i, comparison;
  while (low <= high) {
    i = (low + high) >> 1;
    comparison = cmp(target, array[i]);
    if (comparison < 0) {
      low = i + 1;
    } else if (comparison > 0) {
      high = i - 1;
    } else {
      return i;
    }
  }
  return i;
};

function arrayFrom(seq) {
  return Array.prototype.slice.apply(seq);
};

function DownloadItem(data) {
  var item = this;
  for (var prop in data) {
    item[prop] = data[prop];
  }
  item.startTime = new Date(item.startTime);
  if (item.canResume == undefined) {
    DownloadItem.canResumeHack = true;
  }

  item.div = document.querySelector('body>div.item').cloneNode(true);
  item.div.id = 'item' + item.id;
  item.div.item = item;

  var items_div = document.getElementById('items');
  if ((items_div.childNodes.length == 0) ||
      (item.startTime.getTime() < items_div.childNodes[
       items_div.childNodes.length - 1].item.startTime.getTime())) {
    items_div.appendChild(item.div);
  } else if (item.startTime.getTime() >
             items_div.childNodes[0].item.startTime.getTime()) {
    items_div.insertBefore(item.div, items_div.childNodes[0]);
  } else {
    var adjacent_div = items_div.childNodes[
      binarySearch(arrayFrom(items_div.childNodes),
                   item.startTime.getTime(),
                   function(target, other) {
          return target - other.item.startTime.getTime();
    })];
    var adjacent_item = adjacent_div.item;
    if (adjacent_item.startTime.getTime() < item.startTime.getTime()) {
      items_div.insertBefore(item.div, adjacent_div);
    } else {
      items_div.insertBefore(item.div, adjacent_div.nextSibling);
    }
  }

  item.getElement('referrer').onclick = function() {
    chrome.tabs.create({url: item.referrer});
    return false;
  };
  item.getElement('by-ext').onclick = function() {
    chrome.tabs.create({url: 'chrome://extensions#' + item.byExtensionId});
    return false;
  }
  item.getElement('open-filename').onclick = function() {
    item.open();
    return false;
  };
  item.getElement('pause').onclick = function() {
    item.pause();
    return false;
  };
  item.getElement('cancel').onclick = function() {
    item.cancel();
    return false;
  };
  item.getElement('resume').onclick = function() {
    item.resume();
    return false;
  };
  item.getElement('show-folder').onclick = function() {
    item.show();
    return false;
  };
  item.getElement('remove-file').onclick = function() {
    item.removeFile();
    return false;
  };
  item.getElement('erase').onclick = function() {
    item.erase();
    return false;
  };

  item.more_mousemove = function(evt) {
    var mouse = {x:evt.x, y:evt.y+document.body.scrollTop};
    if (item.getElement('more') &&
        (pointInElement(mouse, item.div) ||
         pointInElement(mouse, item.getElement('more')))) {
      return;
    }
    if (item.getElement('more')) {
      item.getElement('more').hidden = true;
    }
    window.removeEventListener('mousemove', item.more_mousemove);
  };
  [item.div, item.getElement('more')].concat(
      item.getElement('more').children).forEach(function(elem) {
    elem.onmouseover = function() {
      arrayFrom(items_div.children).forEach(function(other) {
        if (other.item != item) {
          other.item.getElement('more').hidden = true;
        }
      });
      item.getElement('more').hidden = false;
      item.getElement('more').style.top =
        (item.div.offsetTop + item.div.offsetHeight) + 'px';
      item.getElement('more').style.left = item.div.offsetLeft + 'px';
      if (window.innerHeight < (parseInt(item.getElement('more').style.top) +
                                item.getElement('more').offsetHeight)) {
        item.getElement('more').style.top = (
          item.div.offsetTop - item.getElement('more').offsetHeight) + 'px';
      }
      window.addEventListener('mousemove', item.more_mousemove);
    };
  });

  if (item.referrer) {
    item.getElement('referrer').href = item.referrer;
  } else {
    item.getElement('referrer').hidden = true;
  }
  item.getElement('url').href = item.url;
  item.getElement('url').innerText = item.url;
  item.render();
}
DownloadItem.canResumeHack = false;

DownloadItem.prototype.getElement = function(name) {
  return document.querySelector('#item' + this.id + ' .' + name);
};

DownloadItem.prototype.render = function() {
  var item = this;
  var now = new Date();
  var in_progress = (item.state == 'in_progress')
  var openable = (item.state != 'interrupted') && item.exists && !item.deleted;

  item.startTime = new Date(item.startTime);
  if (DownloadItem.canResumeHack) {
    item.canResume = in_progress && item.paused;
  }
  if (item.filename) {
    item.basename = item.filename.substring(Math.max(
      item.filename.lastIndexOf('\\'),
      item.filename.lastIndexOf('/')) + 1);
  }
  if (item.estimatedEndTime) {
    item.estimatedEndTime = new Date(item.estimatedEndTime);
  }
  if (item.endTime) {
    item.endTime = new Date(item.endTime);
  }

  if (item.filename && !item.icon_url) {
    chrome.downloads.getFileIcon(
      item.id,
      {'size': 32},
      function(icon_url) {
        item.getElement('icon').hidden = !icon_url;
        if (icon_url) {
          item.icon_url = icon_url;
          item.getElement('icon').src = icon_url;
        }
    });
  }

  item.getElement('removed').style.display = openable ? 'none' : 'inline';
  item.getElement('open-filename').style.display = (
    openable ? 'inline' : 'none');
  item.getElement('in-progress').hidden = !in_progress;
  item.getElement('pause').style.display = (
    !in_progress || item.paused) ? 'none' : 'inline-block';
  item.getElement('resume').style.display = (
    !in_progress || !item.canResume) ? 'none' : 'inline-block';
  item.getElement('cancel').style.display = (
    !in_progress ? 'none' : 'inline-block');
  item.getElement('remove-file').hidden = (
    (item.state != 'complete') ||
    !item.exists ||
    item.deleted ||
    !chrome.downloads.removeFile);
  item.getElement('erase').hidden = in_progress;

  var could_progress = in_progress || item.canResume;
  item.getElement('progress').style.display = (
    could_progress ? 'inline-block' : 'none');
  item.getElement('meter').hidden = !could_progress || !item.totalBytes;

  item.getElement('removed').innerText = item.basename;
  item.getElement('open-filename').innerText = item.basename;

  function setByExtension(show) {
    if (show) {
      item.getElement('by-ext').title = item.byExtensionName;
      item.getElement('by-ext').href =
        'chrome://extensions#' + item.byExtensionId;
      item.getElement('by-ext img').src =
        'chrome://extension-icon/' + item.byExtensionId + '/48/1';
    } else {
      item.getElement('by-ext').hidden = true;
    }
  }
  if (item.byExtensionId && item.byExtensionName) {
    chrome.permissions.contains({permissions: ['management']},
                                function(result) {
      if (result) {
        setByExtension(true);
      } else {
        setByExtension(false);
        if (!localStorage.managementPermissionDenied) {
          document.getElementById('request-management-permission').hidden =
            false;
          document.getElementById('grant-management-permission').onclick =
              function() {
            chrome.permissions.request({permissions: ['management']},
                                      function(granted) {
              setByExtension(granted);
              if (!granted) {
                localStorage.managementPermissionDenied = true;
              }
            });
            return false;
          };
        }
      }
    });
  } else {
    setByExtension(false);
  }

  if (!item.getElement('error').hidden) {
    if (item.error) {
      // TODO(benjhayden) When https://codereview.chromium.org/16924017/ is
      // released, set minimum_chrome_version and remove the error_N messages.
      item.getElement('error').innerText = chrome.i18n.getMessage(
          'error_' + item.error);
      if (!item.getElement('error').innerText) {
        item.getElement('error').innerText = item.error;
      }
    } else if (!openable) {
      item.getElement('error').innerText = chrome.i18n.getMessage(
          'errorRemoved');
    }
  }

  item.getElement('complete-size').innerText = formatBytes(
    item.bytesReceived);
  if (item.totalBytes && (item.state != 'complete')) {
    item.getElement('progress').innerText = (
      item.getElement('complete-size').innerText + '/' +
      formatBytes(item.totalBytes));
    item.getElement('meter').children[0].style.width = parseInt(
        100 * item.bytesReceived / item.totalBytes) + '%';
  }

  if (in_progress) {
    if (item.estimatedEndTime && !item.paused) {
      var openWhenComplete = false;
      try {
        openWhenComplete = JSON.parse(localStorage.openWhenComplete).indexOf(
            item.id) >= 0;
      } catch (e) {
      }
      item.getElement('time-left').innerText = formatTimeLeft(
          openWhenComplete, item.estimatedEndTime.getTime() - now.getTime());
    } else {
      item.getElement('time-left').innerText = String.fromCharCode(160);
    }
  }

  if (item.startTime) {
    item.getElement('start-time').innerText = formatDateTime(
        item.startTime);
  }

  ratchetWidth(item.getElement('icon').offsetWidth +
               item.getElement('file-url').offsetWidth +
               item.getElement('cancel').offsetWidth +
               item.getElement('pause').offsetWidth +
               item.getElement('resume').offsetWidth);
  ratchetWidth(item.getElement('more').offsetWidth);

  this.maybeAccept();
};

DownloadItem.prototype.onChanged = function(delta) {
  for (var key in delta) {
    if (key != 'id') {
      this[key] = delta[key].current;
    }
  }
  this.render();
  if (delta.state) {
    setLastOpened();
  }
  if ((this.state == 'in_progress') && !this.paused) {
    DownloadManager.startPollingProgress();
  }
};

DownloadItem.prototype.onErased = function() {
  window.removeEventListener('mousemove', this.more_mousemove);
  document.getElementById('items').removeChild(this.div);
};

DownloadItem.prototype.show = function() {
  chrome.downloads.show(this.id);
};

DownloadItem.prototype.open = function() {
  if (this.state == 'complete') {
    chrome.downloads.open(this.id);
    return;
  }
  chrome.runtime.sendMessage({openWhenComplete:this.id});
};

DownloadItem.prototype.removeFile = function() {
  chrome.downloads.removeFile(this.id);
  this.deleted = true;
  this.render();
};

DownloadItem.prototype.erase = function() {
  chrome.downloads.erase({id: this.id});
};

DownloadItem.prototype.pause = function() {
  chrome.downloads.pause(this.id);
};

DownloadItem.prototype.resume = function() {
  chrome.downloads.resume(this.id);
};

DownloadItem.prototype.cancel = function() {
  chrome.downloads.cancel(this.id);
};

DownloadItem.prototype.maybeAccept = function() {
  // This function is safe to call at any time for any item, and it will always
  // do the right thing, which is to display the danger prompt only if the item
  // is in_progress and dangerous, and if the prompt is not already displayed.
  if ((this.state != 'in_progress') ||
      (this.danger == 'safe') ||
      (this.danger == 'accepted') ||
      DownloadItem.prototype.maybeAccept.accepting_danger) {
    return;
  }
  ratchetWidth(400);
  ratchetHeight(200);
  DownloadItem.prototype.maybeAccept.accepting_danger = true;
  // On Mac, window.onload is run while the popup is animating in, before it is
  // considered "visible". Prompts will not be displayed over an invisible
  // window, so the popup will become stuck. Just wait a little bit for the
  // window to finish animating in. http://crbug.com/280107
  // This has been fixed, so this setTimeout can be removed when the fix has
  // been released to stable, and minimum_chrome_version can be set.
  var id = this.id;
  setTimeout(function() {
    chrome.downloads.acceptDanger(id, function() {
      DownloadItem.prototype.maybeAccept.accepting_danger = false;
      arrayFrom(document.getElementById('items').childNodes).forEach(
        function(item_div) { item_div.item.maybeAccept(); });
    });
  }, 500);
};
DownloadItem.prototype.maybeAccept.accepting_danger = false;

var DownloadManager = {};

DownloadManager.showingOlder = false;

DownloadManager.getItem = function(id) {
  var item_div = document.getElementById('item' + id);
  return item_div ? item_div.item : null;
};

DownloadManager.getOrCreate = function(data) {
  var item = DownloadManager.getItem(data.id);
  return item ? item : new DownloadItem(data);
};

DownloadManager.forEachItem = function(cb) {
  // Calls cb(item, index) in the order that they are displayed, i.e. in order
  // of decreasing startTime.
  arrayFrom(document.getElementById('items').childNodes).forEach(
    function(item_div, index) { cb(item_div.item, index); });
};

DownloadManager.startPollingProgress = function() {
  if (DownloadManager.startPollingProgress.tid < 0) {
    DownloadManager.startPollingProgress.tid = setTimeout(
      DownloadManager.startPollingProgress.pollProgress,
      DownloadManager.startPollingProgress.MS);
  }
}
DownloadManager.startPollingProgress.MS = 200;
DownloadManager.startPollingProgress.tid = -1;
DownloadManager.startPollingProgress.pollProgress = function() {
  DownloadManager.startPollingProgress.tid = -1;
  chrome.downloads.search({state: 'in_progress', paused: false},
      function(results) {
    if (!results.length)
      return;
    results.forEach(function(result) {
      var item = DownloadManager.getOrCreate(result);
      for (var prop in result) {
        item[prop] = result[prop];
      }
      item.render();
      if ((item.state == 'in_progress') && !item.paused) {
        DownloadManager.startPollingProgress();
      }
    });
  });
};

DownloadManager.showNew = function() {
  var any_items = (document.getElementById('items').childNodes.length > 0);
  document.getElementById('empty').style.display =
    any_items ? 'none' : 'inline-block';
  document.getElementById('head').style.borderBottomWidth =
    (any_items ? 1 : 0) + 'px';
  document.getElementById('clear-all').hidden = !any_items;

  var query_search = document.getElementById('q');
  query_search.hidden = !any_items;

  if (!any_items) {
    return;
  }
  var old_ms = (new Date()).getTime() - kOldMs;
  var any_hidden = false;
  var any_showing = false;
  // First show up to kShowNewMax items newer than kOldMs. If there aren't any
  // items newer than kOldMs, then show up to kShowNewMax items of any age. If
  // there are any hidden items, show the Show Older button.
  DownloadManager.forEachItem(function(item, index) {
    item.div.hidden = !DownloadManager.showingOlder && (
      (item.startTime.getTime() < old_ms) || (index >= kShowNewMax));
    any_hidden = any_hidden || item.div.hidden;
    any_showing = any_showing || !item.div.hidden;
  });
  if (!any_showing) {
    any_hidden = false;
    DownloadManager.forEachItem(function(item, index) {
      item.div.hidden = !DownloadManager.showingOlder && (index >= kShowNewMax);
      any_hidden = any_hidden || item.div.hidden;
      any_showing = any_showing || !item.div.hidden;
    });
  }
  document.getElementById('older').hidden = !any_hidden;

  query_search.focus();
};

DownloadManager.showOlder = function() {
  DownloadManager.showingOlder = true;
  var loading_older_span = document.getElementById('loading-older');
  document.getElementById('older').hidden = true;
  loading_older_span.hidden = false;
  chrome.downloads.search({}, function(results) {
    results.forEach(function(result) {
      var item = DownloadManager.getOrCreate(result);
      item.div.hidden = false;
    });
    loading_older_span.hidden = true;
  });
};

DownloadManager.onSearch = function() {
  // split string by space, but ignore space in quotes
  // http://stackoverflow.com/questions/16261635
  var query = document.getElementById('q').value.match(/(?:[^\s"]+|"[^"]*")+/g);
  if (!query) {
    DownloadManager.showNew();
    document.getElementById('search-zero').hidden = true;
  } else {
    query = query.map(function(term) {
      // strip quotes
      return (term.match(/\s/) &&
              term[0].match(/["']/) &&
              term[term.length - 1] == term[0]) ?
        term.substr(1, term.length - 2) : term;
    });
    var searching = document.getElementById('searching');
    searching.hidden = false;
    chrome.downloads.search({query: query}, function(results) {
      document.getElementById('older').hidden = true;
      DownloadManager.forEachItem(function(item) {
        item.div.hidden = true;
      });
      results.forEach(function(result) {
        DownloadManager.getOrCreate(result).div.hidden = false;
      });
      searching.hidden = true;
      document.getElementById('search-zero').hidden = (results.length != 0);
    });
  }
};

DownloadManager.clearAll = function() {
  DownloadManager.forEachItem(function(item) {
    if (!item.div.hidden) {
      item.erase();
      // The onErased handler should circle back around to loadItems.
    }
  });
};

var kShowNewMax = 50;
var kOldMs = 1000 * 60 * 60 * 24 * 7;

// These settings can be tuned by modifying localStorage in dev-tools.
if ('kShowNewMax' in localStorage) {
  kShowNewMax = parseInt(localStorage.kShowNewMax);
}
if ('kOldMs' in localStorage) {
  kOldMs = parseInt(localStorage.kOldMs);
}

DownloadManager.loadItems = function() {
  // Request up to kShowNewMax + 1, but only display kShowNewMax; the +1 is a
  // probe to see if there are any older downloads.
  // TODO(benjhayden) When https://codereview.chromium.org/16924017/ is
  // released, set minimum_chrome_version and remove this try/catch.
  try {
    chrome.downloads.search({
        orderBy: ['-startTime'],
        limit: kShowNewMax + 1},
      function(results) {
        DownloadManager.loadItems.items = results;
        DownloadManager.loadItems.onLoaded();
    });
  } catch (exc) {
    chrome.downloads.search({
        orderBy: '-startTime',
        limit: kShowNewMax + 1},
      function(results) {
        DownloadManager.loadItems.items = results;
        DownloadManager.loadItems.onLoaded();
    });
  }
};
DownloadManager.loadItems.items = [];
DownloadManager.loadItems.window_loaded = false;

DownloadManager.loadItems.onLoaded = function() {
  if (!DownloadManager.loadItems.window_loaded) {
    return;
  }
  DownloadManager.loadItems.items.forEach(function(item) {
    DownloadManager.getOrCreate(item);
  });
  DownloadManager.loadItems.items = [];
  DownloadManager.showNew();
};

DownloadManager.loadItems.onWindowLoaded = function() {
  DownloadManager.loadItems.window_loaded = true;
  DownloadManager.loadItems.onLoaded();
};

// If this extension is installed on a stable-channel chrome, where the
// downloads API is not available, do not use the downloads API, and link to the
// beta channel.
if (chrome.downloads) {
  // Start searching ASAP, don't wait for onload.
  DownloadManager.loadItems();

  chrome.downloads.onCreated.addListener(function(item) {
    DownloadManager.getOrCreate(item);
    DownloadManager.showNew();
    DownloadManager.startPollingProgress();
  });

  chrome.downloads.onChanged.addListener(function(delta) {
    var item = DownloadManager.getItem(delta.id);
    if (item) {
      item.onChanged(delta);
    }
  });

  chrome.downloads.onErased.addListener(function(id) {
    var item = DownloadManager.getItem(id);
    if (!item) {
      return;
    }
    item.onErased();
    DownloadManager.loadItems();
  });

  window.onload = function() {
    ratchetWidth(
      document.getElementById('q-outer').offsetWidth +
      document.getElementById('clear-all').offsetWidth +
      document.getElementById('open-folder').offsetWidth);
    setLastOpened();
    loadI18nMessages();
    DownloadManager.loadItems.onWindowLoaded();
    document.getElementById('older').onclick = function() {
      DownloadManager.showOlder();
      return false;
    };
    document.getElementById('q').onsearch = function() {
      DownloadManager.onSearch();
    };
    document.getElementById('clear-all').onclick = function() {
      DownloadManager.clearAll();
      return false;
    };
    if (chrome.downloads.showDefaultFolder) {
      document.getElementById('open-folder').onclick = function() {
        chrome.downloads.showDefaultFolder();
        return false;
      };
    } else {
      document.getElementById('open-folder').hidden = true;
    }
  };
} else {
  // The downloads API is not available.
  // TODO(benjhayden) Remove this when minimum_chrome_version is set.
  window.onload = function() {
    loadI18nMessages();
    var bad_version = document.getElementById('bad-chrome-version');
    bad_version.hidden = false;
    bad_version.onclick = function() {
      chrome.tabs.create({url: bad_version.href});
      return false;
    };
    document.getElementById('empty').style.display = 'none';
    document.getElementById('q').style.display = 'none';
    document.getElementById('open-folder').style.display = 'none';
    document.getElementById('clear-all').style.display = 'none';
  };
}
