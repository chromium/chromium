// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const REQUEST_TO_COMM_CHANNEL_1 = 'connect';
const REQUEST_TO_COMM_CHANNEL_2 = 'connect_request';
const RESPONSE_FROM_COMM_CHANNEL_1 = 'connected';
const RESPONSE_FROM_COMM_CHANNEL_2 = 'connected_response';

function createWebview() {
  const webview = document.createElement('webview');
  document.body.appendChild(webview);
  return webview;
}

function onGetBackgroundExecuted(results) {
  chrome.send('testResult', [results.length === 1 && results[0] === 'red']);
}

function testExecuteScriptCode(url) {
  const webview = createWebview();

  const onSetBackgroundExecuted = function() {
    webview.executeScript(
        {code: 'document.body.style.backgroundColor;'},
        onGetBackgroundExecuted);
  };

  const onLoadStop = function() {
    webview.executeScript(
        {code: 'document.body.style.backgroundColor = \'red\';'},
        onSetBackgroundExecuted);
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = url;
}

function testExecuteScriptCodeFromFile(url) {
  const webview = createWebview();

  const onSetBackgroundExecuted = function() {
    webview.executeScript(
        {code: 'document.body.style.backgroundColor;'},
        onGetBackgroundExecuted);
  };

  const onLoadStop = function() {
    webview.executeScript(
        {file: 'test/webview_execute_script.js'}, onSetBackgroundExecuted);
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = url;
}

// This test verifies that a content script will be injected to the webview when
// the webview is navigated to a page that matches the URL pattern defined in
// the content sript.
function testAddContentScript(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {
      files: ['test/inject_comm_channel.js', 'test/inject_comm_channel_2.js'],
    },
    run_at: 'document_start',
  }]);

  webview.addEventListener('loadstop', function() {
    console.info('Step 2: postMessage to build connection.');
    const msg = [REQUEST_TO_COMM_CHANNEL_1];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  });

  window.addEventListener('message', function(e) {
    if (e.source !== webview.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
      console.info(
          'Step 3: A communication channel has been established with webview.');
      chrome.send('testResult', [true]);
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  webview.src = url;
  document.body.appendChild(webview);
}

// Adds two content scripts with the same URL pattern to <webview> at the same
// time. This test verifies that both scripts are injected when the <webview>
// navigates to a URL that matches the URL pattern.
function testAddMultiContentScripts(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts(myrule1 & myrule2)');
  webview.addContentScripts([
    {
      name: 'myrule1',
      matches: ['http://*/empty*'],
      js: {files: ['test/inject_comm_channel.js']},
      run_at: 'document_start',
    },
    {
      name: 'myrule2',
      matches: ['http://*/empty*'],
      js: {files: ['test/inject_comm_channel_2.js']},
      run_at: 'document_start',
    },
  ]);

  webview.addEventListener('loadstop', function() {
    console.info('Step 2: postMessage to build connection.');
    const msg1 = [REQUEST_TO_COMM_CHANNEL_1];
    webview.contentWindow.postMessage(JSON.stringify(msg1), '*');
    console.info(
        'Step 3: postMessage to build connection to the other script.');
    const msg2 = [REQUEST_TO_COMM_CHANNEL_2];
    webview.contentWindow.postMessage(JSON.stringify(msg2), '*');
  });

  let response_1 = false;
  let response_2 = false;
  window.addEventListener('message', function(e) {
    if (e.source !== webview.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
      console.info(
          'Step 4: A communication channel has been established with webview.');
      response_1 = true;
      if (response_1 && response_2) {
        chrome.send('testResult', [true]);
      }
      return;
    } else if (data[0] === RESPONSE_FROM_COMM_CHANNEL_2) {
      console.info(
          'Step 5: A communication channel has been established with webview.');
      response_2 = true;
      if (response_1 && response_2) {
        chrome.send('testResult', [true]);
      }
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  webview.src = url;
  document.body.appendChild(webview);
}

// Adds a content script to <webview> and navigates. After seeing the script is
// injected, we add another content script with the same name to the <webview>.
// This test verifies that the second script will replace the first one and be
// injected after navigating the <webview>. Meanwhile, the <webview> shouldn't
// get any message from the first script anymore.
function testAddContentScriptWithSameNameShouldOverwriteTheExistingOne(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts(myrule1)');
  webview.addContentScripts([{
    name: 'myrule1',
    matches: ['http://*/empty*'],
    js: {files: ['test/inject_comm_channel.js']},
    run_at: 'document_start',
  }]);
  let connect_script_1 = true;
  let connect_script_2 = false;

  webview.addEventListener('loadstop', function() {
    if (connect_script_1) {
      const msg1 = [REQUEST_TO_COMM_CHANNEL_1];
      webview.contentWindow.postMessage(JSON.stringify(msg1), '*');
      connect_script_1 = false;
    }
    if (connect_script_2) {
      const msg2 = [REQUEST_TO_COMM_CHANNEL_2];
      webview.contentWindow.postMessage(JSON.stringify(msg2), '*');
      connect_script_2 = false;
    }
  });

  let should_get_response_from_script_1 = true;
  window.addEventListener('message', function(e) {
    if (e.source !== webview.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
      if (should_get_response_from_script_1) {
        console.info(
            'Step 2: A communication channel has been established with webview.');
        console.info(
            'Step 3: <webview>.addContentScripts() with a updated' +
            ' \'myrule1\'');
        webview.addContentScripts([{
          name: 'myrule1',
          matches: ['http://*/empty*'],
          js: {files: ['test/inject_comm_channel_2.js']},
          run_at: 'document_start',
        }]);
        connect_script_2 = true;
        should_get_response_from_script_1 = false;
        webview.src = url;
      } else {
        chrome.send('testResult', [false]);
      }
      return;
    } else if (data[0] === RESPONSE_FROM_COMM_CHANNEL_2) {
      console.info(
          'Step 4: Another communication channel has been established ' +
          'with webview.');
      setTimeout(function() {
        chrome.send('testResult', [true]);
      }, 0);
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  webview.src = url;
  document.body.appendChild(webview);
}

// There are two <webview>s are added to the DOM, and we add a content script
// to one of them. This test verifies that the script won't be injected in
// the other <webview>.
function testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView(url) {
  const webview1 = document.createElement('webview');
  const webview2 = document.createElement('webview');

  console.info('Step 1: call <webview1>.addContentScripts.');
  webview1.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {files: ['test/inject_comm_channel.js']},
    run_at: 'document_start',
  }]);

  webview2.addEventListener('loadstop', function() {
    console.info('Step 2: webview2 requests to build communication channel.');
    const msg = [REQUEST_TO_COMM_CHANNEL_1];
    webview2.contentWindow.postMessage(JSON.stringify(msg), '*');
    setTimeout(function() {
      chrome.send('testResult', [true]);
    }, 0);
  });

  window.addEventListener('message', function(e) {
    if (e.source !== webview2.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
      chrome.send('testResult', [false]);
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  webview1.src = url;
  webview2.src = url;
  document.body.appendChild(webview1);
  document.body.appendChild(webview2);
}

// Adds a content script to <webview> and navigates to a URL that matches the
// URL pattern defined in the script. After the first navigation, we remove this
// script from the <webview> and navigates to the same URL. This test verifies
// taht the script is injected during the first navigation, but isn't injected
// after removing it.
function testAddAndRemoveContentScripts(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {files: ['test/inject_comm_channel.js']},
    run_at: 'document_start',
  }]);

  let should_get_response_from_script_1 = true;

  let count = 0;
  webview.addEventListener('loadstop', function() {
    if (count === 0) {
      console.info('Step 2: post message to build connect.');
      const msg = [REQUEST_TO_COMM_CHANNEL_1];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      ++count;
    } else if (count === 1) {
      console.info('Step 5: post message to build connect again.');
      const msg = [REQUEST_TO_COMM_CHANNEL_1];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      setTimeout(function() {
        chrome.send('testResult', [true]);
      }, 0);
    }
  });

  window.addEventListener('message', function(e) {
    if (e.source !== webview.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1 &&
        should_get_response_from_script_1) {
      console.info(
          'Step 3: A communication channel has been established ' +
          'with webview.');
      should_get_response_from_script_1 = false;
      console.info('Step 4: call <webview>.removeContentScripts and navigate.');
      webview.removeContentScripts();
      webview.src = url;
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  webview.src = url;
  document.body.appendChild(webview);
}

// This test verifies that the addContentScripts API works with the new window
// API.
function testAddContentScriptsWithNewWindowAPI(url) {
  const webview = document.createElement('webview');

  let newwebview;
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    newwebview = document.createElement('webview');

    console.info('Step 2: call newwebview.addContentScripts.');
    newwebview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/guest_from_opener*'],
      js: {files: ['test/inject_comm_channel.js']},
      run_at: 'document_start',
    }]);

    newwebview.addEventListener('loadstop', function(evt) {
      const msg = [REQUEST_TO_COMM_CHANNEL_1];
      console.info(
          'Step 4: new webview postmessage to build communication ' +
          'channel.');
      newwebview.contentWindow.postMessage(JSON.stringify(msg), '*');
    });

    document.body.appendChild(newwebview);
    // attach the new window to the new <webview>.
    console.info('Step 3: attaches the new webview.');
    e.window.attach(newwebview);
  });

  window.addEventListener('message', function(e) {
    if (!newwebview || e.source !== newwebview.contentWindow) {
      return;
    }
    const data = JSON.parse(e.data);
    if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1 &&
        e.source === newwebview.contentWindow) {
      console.info(
          'Step 5: a communication channel has been established ' +
          'with the new webview.');
      chrome.send('testResult', [true]);
      return;
    } else {
      chrome.send('testResult', [false]);
      return;
    }
    console.info('Unexpected message: \'' + data[0] + '\'');
    chrome.send('testResult', [false]);
  });

  console.info('Step 1: navigates the webview to window open guest URL.');
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// Adds a content script to <webview>. This test verifies that the script is
// injected after terminate and reload <webview>.
function testContentScriptIsInjectedAfterTerminateAndReloadWebView(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {files: ['test/webview_execute_script.js']},
    run_at: 'document_end',
  }]);

  let count = 0;
  webview.addEventListener('loadstop', function() {
    if (count === 0) {
      console.info('Step 2: call webview.terminate().');
      webview.terminate();
      ++count;
      return;
    } else if (count === 1) {
      console.info('Step 4: call <webview>.executeScript to check result.');
      webview.executeScript(
          {code: 'document.body.style.backgroundColor;'},
          onGetBackgroundExecuted);
    }
  });

  webview.addEventListener('exit', function() {
    console.info('Step 3: call webview.reload().');
    webview.reload();
  });

  webview.src = url;
  document.body.appendChild(webview);
}

// This test verifies the content script won't be removed when the guest is
// destroyed, i.e., removed <webview> from the DOM.
function testContentScriptExistsAsLongAsWebViewTagExists(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {files: ['test/webview_execute_script.js']},
    run_at: 'document_end',
  }]);

  let count = 0;
  webview.addEventListener('loadstop', function() {
    if (count === 0) {
      console.info('Step 2: check the result of content script injected.');
      webview.executeScript(
          {code: 'document.body.style.backgroundColor;'}, function(results) {
            assertEquals(1, results.length);
            assertEquals('red', results[0]);

            console.info('Step 3: remove webview from the DOM.');
            document.body.removeChild(webview);
            console.info('Step 4: add webview back to the DOM.');
            document.body.appendChild(webview);
            ++count;
          });
    } else if (count === 1) {
      console.info(
          'Step 5: check the result of content script injected again.');
      webview.executeScript(
          {code: 'document.body.style.backgroundColor;'},
          onGetBackgroundExecuted);
    }
  });

  webview.src = url;
  document.body.appendChild(webview);
}

function testAddContentScriptWithCode(url) {
  const webview = document.createElement('webview');

  console.info('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts([{
    name: 'myrule',
    matches: ['http://*/empty*'],
    js: {code: 'document.body.style.backgroundColor = \'red\';'},
    run_at: 'document_end',
  }]);

  webview.addEventListener('loadstop', function() {
    console.info('Step 2: call webview.executeScript() to check result.');
    webview.executeScript(
        {code: 'document.body.style.backgroundColor;'},
        onGetBackgroundExecuted);
  });

  webview.src = url;
  document.body.appendChild(webview);
}

function testDragAndDropToInput() {
  const css = document.createElement('style');
  css.type = 'text/css';
  css.innerHTML = 'html, body { height: 400px }';
  document.body.appendChild(css);

  const contents = document.getElementById('contents');
  while (contents.childElementCount) {
    contents.removeChild(contents.firstChild);
  }
  const webview = document.createElement('webview');

  webview.id = 'webview';
  webview.style = 'width:640px; height:480px';

  window.addEventListener('message', function(e) {
    const data = JSON.parse(e.data)[0];
    console.info('get message: ' + data);
    if (data === 'connected') {
      chrome.send('testResult', [true]);
      return;
    }

    chrome.send(data);
  });

  webview.addEventListener('loadstop', function(e) {
    if (webview.src !== 'about:blank') {
      return;
    }
    console.info('load stop of src = :' + webview.src);
    webview.executeScript(
        {file: 'test/draganddroptoinput.js'}, function(results) {
          console.info('finish guest load');
          webview.contentWindow.postMessage(
              JSON.stringify(['create-channel']), '*');
        });
  });

  // For debug messages from guests.
  webview.addEventListener('consolemessage', function(e) {
    console.info('[Guest]: ' + e.message);
  });

  webview.src = 'about:blank';
  contents.appendChild(webview);
}
