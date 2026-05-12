// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('WebviewContentScriptTest', function() {
  const REQUEST_TO_COMM_CHANNEL_1 = 'connect';
  const REQUEST_TO_COMM_CHANNEL_2 = 'connect_request';
  const RESPONSE_FROM_COMM_CHANNEL_1 = 'connected';
  const RESPONSE_FROM_COMM_CHANNEL_2 = 'connected_response';

  function createWebview(): chrome.webviewTag.WebView {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    document.body.appendChild(webview);
    return webview;
  }

  function checkBackgroundColor(webview: chrome.webviewTag.WebView):
      Promise<void> {
    return new Promise<void>(resolve => {
      webview.executeScript(
          {code: 'document.body.style.backgroundColor;'}, (results: any[]) => {
            assertEquals(1, results.length);
            assertEquals('red', results[0]);
            resolve();
          });
    });
  }

  function executeScript(
      webview: chrome.webviewTag.WebView, details: any): Promise<any[]> {
    return new Promise<any[]>(resolve => {
      webview.executeScript(details, (results: any[]) => {
        resolve(results);
      });
    });
  }

  function getWebviewUrl(): string {
    return (window as unknown as Window & {webviewUrl: string}).webviewUrl;
  }

  function getWebSocketPort(): number {
    return (window as unknown as Window & {webSocketPort: number})
        .webSocketPort;
  }

  function getWebTransportPort(): number {
    return (window as unknown as Window & {webTransportPort: number})
        .webTransportPort;
  }

  test('ExecuteScriptCode', async () => {
    const webview = createWebview();

    const whenLoaded = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', () => {
        webview.executeScript(
            {code: 'document.body.style.backgroundColor = \'red\';'},
            (_results: any[]) => {
              resolve();
            });
      });
    });
    webview.src = getWebviewUrl();
    await whenLoaded;
    await checkBackgroundColor(webview);
  });

  test('ExecuteScriptCodeFromFile', async () => {
    const webview = createWebview();
    const whenLoaded = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', () => {
        webview.executeScript({file: 'test/webview_execute_script.js'}, () => {
          resolve();
        });
      });
    });
    webview.src = getWebviewUrl();
    await whenLoaded;
    await checkBackgroundColor(webview);
  });

  // This test verifies that a content script will be injected to the webview
  // when the webview is navigated to a page that matches the URL pattern
  // defined in the content script.
  test('AddContentScript', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts.');
    webview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/empty*'],
      js: {
        files: ['test/inject_comm_channel.js', 'test/inject_comm_channel_2.js'],
      },
      run_at: 'document_start' as chrome.extensionTypes.RunAt,
    }]);

    webview.addEventListener('loadstop', function() {
      console.info('Step 2: postMessage to build connection.');
      const msg = [REQUEST_TO_COMM_CHANNEL_1];
      assertTrue(!!webview.contentWindow);
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
    });

    const whenMessageReceived = new Promise<void>(resolve => {
      window.addEventListener('message', function(e) {
        if (e.source !== webview.contentWindow) {
          return;
        }
        const data = JSON.parse(e.data);
        assertEquals(
            RESPONSE_FROM_COMM_CHANNEL_1, data[0],
            'Unexpected message: \'' + data[0] + '\'');
        console.info(
            'Step 3: A communication channel has been established with ' +
            'webview.');
        resolve();
      });
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await whenMessageReceived;
  });

  // Adds two content scripts with the same URL pattern to <webview> at the same
  // time. This test verifies that both scripts are injected when the <webview>
  // navigates to a URL that matches the URL pattern.
  test('AddMultiContentScripts', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts(myrule1 & myrule2)');
    webview.addContentScripts([
      {
        name: 'myrule1',
        matches: ['http://*/empty*'],
        js: {files: ['test/inject_comm_channel.js']},
        run_at: 'document_start' as chrome.extensionTypes.RunAt,
      },
      {
        name: 'myrule2',
        matches: ['http://*/empty*'],
        js: {files: ['test/inject_comm_channel_2.js']},
        run_at: 'document_start' as chrome.extensionTypes.RunAt,
      },
    ]);

    webview.addEventListener('loadstop', function() {
      console.info('Step 2: postMessage to build connection.');
      const msg1 = [REQUEST_TO_COMM_CHANNEL_1];
      webview.contentWindow!.postMessage(JSON.stringify(msg1), '*');
      console.info(
          'Step 3: postMessage to build connection to the other script.');
      const msg2 = [REQUEST_TO_COMM_CHANNEL_2];
      webview.contentWindow!.postMessage(JSON.stringify(msg2), '*');
    });

    let response1 = false;
    let response2 = false;
    const whenResponse = new Promise<void>(resolve => {
      window.addEventListener('message', function(e) {
        if (e.source !== webview.contentWindow) {
          return;
        }
        const data = JSON.parse(e.data);
        if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
          console.info(
              'Step 4: A communication channel has been established with ' +
              'webview.');
          response1 = true;
          if (response1 && response2) {
            resolve();
          }
          return;
        }
        assertEquals(
            RESPONSE_FROM_COMM_CHANNEL_2, data[0],
            'Unexpected message: \'' + data[0] + '\'');
        console.info(
            'Step 5: A communication channel has been established with ' +
            'webview.');
        response2 = true;
        if (response1 && response2) {
          resolve();
        }
        return;
      });
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await whenResponse;
  });

  // Adds a content script to <webview> and navigates. After seeing the script
  // is injected, we add another content script with the same name to the
  // <webview>. This test verifies that the second script will replace the first
  // one and be injected after navigating the <webview>. Meanwhile, the
  // <webview> shouldn't get any message from the first script anymore.
  test(
      'AddContentScriptWithSameNameShouldOverwriteTheExistingOne', async () => {
        const webview =
            document.createElement('webview') as chrome.webviewTag.WebView;

        console.info('Step 1: call <webview>.addContentScripts(myrule1)');
        webview.addContentScripts([{
          name: 'myrule1',
          matches: ['http://*/empty*'],
          js: {files: ['test/inject_comm_channel.js']},
          run_at: 'document_start' as chrome.extensionTypes.RunAt,
        }]);
        let connectScript1 = true;
        let connectScript2 = false;

        webview.addEventListener('loadstop', function() {
          if (connectScript1) {
            const msg1 = [REQUEST_TO_COMM_CHANNEL_1];
            webview.contentWindow!.postMessage(JSON.stringify(msg1), '*');
            connectScript1 = false;
          }
          if (connectScript2) {
            const msg2 = [REQUEST_TO_COMM_CHANNEL_2];
            webview.contentWindow!.postMessage(JSON.stringify(msg2), '*');
            connectScript2 = false;
          }
        });

        let shouldGetResponseFromScript1 = true;
        const whenChannel2ResponseReceived = new Promise<void>(resolve => {
          window.addEventListener('message', function(e) {
            if (e.source !== webview.contentWindow) {
              return;
            }
            const data = JSON.parse(e.data);
            if (data[0] === RESPONSE_FROM_COMM_CHANNEL_1) {
              assertTrue(shouldGetResponseFromScript1);
              console.info(
                  'Step 2: A communication channel has been established with ' +
                  'webview.');
              console.info(
                  'Step 3: <webview>.addContentScripts() with a updated' +
                  ' \'myrule1\'');
              webview.addContentScripts([{
                name: 'myrule1',
                matches: ['http://*/empty*'],
                js: {files: ['test/inject_comm_channel_2.js']},
                run_at: 'document_start' as chrome.extensionTypes.RunAt,
              }]);
              connectScript2 = true;
              shouldGetResponseFromScript1 = false;
              webview.src = getWebviewUrl();
              return;
            }
            assertEquals(
                RESPONSE_FROM_COMM_CHANNEL_2, data[0],
                'Unexpected message : \'' + data[0] + '\'');
            console.info(
                'Step 4: Another communication channel has been established ' +
                'with webview.');
            resolve();
          });
        });

        webview.src = getWebviewUrl();
        document.body.appendChild(webview);
        await whenChannel2ResponseReceived;
      });

  // There are two <webview>s are added to the DOM, and we add a content script
  // to one of them. This test verifies that the script won't be injected in
  // the other <webview>.
  test(
      'AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView',
      async () => {
        const webview1 =
            document.createElement('webview') as chrome.webviewTag.WebView;
        const webview2 =
            document.createElement('webview') as chrome.webviewTag.WebView;

        console.info('Step 1: call <webview1>.addContentScripts.');
        webview1.addContentScripts([{
          name: 'myrule',
          matches: ['http://*/empty*'],
          js: {files: ['test/inject_comm_channel.js']},
          run_at: 'document_start' as chrome.extensionTypes.RunAt,
        }]);

        const whenMessagePosted = new Promise<void>(resolve => {
          webview2.addEventListener('loadstop', function() {
            console.info(
                'Step 2: webview2 requests to build communication channel.');
            const msg = [REQUEST_TO_COMM_CHANNEL_1];
            webview2.contentWindow!.postMessage(JSON.stringify(msg), '*');
            setTimeout(function() {
              resolve();
            }, 0);
          });
        });

        window.addEventListener('message', function(e) {
          assertNotEquals(
              webview2.contentWindow, e.source,
              'Unexpected message : \'' + JSON.parse(e.data)[0] + '\'');
        });
        webview1.src = getWebviewUrl();
        webview2.src = getWebviewUrl();
        document.body.appendChild(webview1);
        document.body.appendChild(webview2);
        await whenMessagePosted;
      });

  // Adds a content script to <webview> and navigates to a URL that matches the
  // URL pattern defined in the script. After the first navigation, we remove
  // this script from the <webview> and navigates to the same URL. This test
  // verifies that the script is injected during the first navigation, but isn't
  // injected after removing it.
  test('AddAndRemoveContentScripts', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts.');
    webview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/empty*'],
      js: {files: ['test/inject_comm_channel.js']},
      run_at: 'document_start' as chrome.extensionTypes.RunAt,
    }]);

    let shouldGetResponseFromScript1 = true;

    let count = 0;
    const whenPostSecondMessage = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', function() {
        if (count === 0) {
          console.info('Step 2: post message to build connect.');
          const msg = [REQUEST_TO_COMM_CHANNEL_1];
          webview.contentWindow!.postMessage(JSON.stringify(msg), '*');
          ++count;
        } else if (count === 1) {
          console.info('Step 5: post message to build connect again.');
          const msg = [REQUEST_TO_COMM_CHANNEL_1];
          webview.contentWindow!.postMessage(JSON.stringify(msg), '*');
          setTimeout(function() {
            resolve();
          }, 0);
        }
      });
    });

    window.addEventListener('message', function(e) {
      if (e.source !== webview.contentWindow) {
        return;
      }
      const data = JSON.parse(e.data);
      assertTrue(shouldGetResponseFromScript1);
      assertEquals(
          RESPONSE_FROM_COMM_CHANNEL_1, data[0],
          'Unexpected message: \'' + data[0] + '\'');
      console.info(
          'Step 3: A communication channel has been established ' +
          'with webview.');
      shouldGetResponseFromScript1 = false;
      console.info('Step 4: call <webview>.removeContentScripts and navigate.');
      webview.removeContentScripts();
      webview.src = getWebviewUrl();
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await whenPostSecondMessage;
  });

  // This test verifies that the addContentScripts API works with the new window
  // API.
  test('AddContentScriptsWithNewWindowAPI', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    let newwebview: chrome.webviewTag.WebView;
    webview.addEventListener('newwindow', function(e) {
      e.preventDefault();
      newwebview =
          document.createElement('webview') as chrome.webviewTag.WebView;

      console.info('Step 2: call newwebview.addContentScripts.');
      newwebview.addContentScripts([{
        name: 'myrule',
        matches: ['http://*/guest_from_opener*'],
        js: {files: ['test/inject_comm_channel.js']},
        run_at: 'document_start' as chrome.extensionTypes.RunAt,
      }]);

      newwebview.addEventListener('loadstop', function() {
        const msg = [REQUEST_TO_COMM_CHANNEL_1];
        console.info(
            'Step 4: new webview postmessage to build communication ' +
            'channel.');
        newwebview.contentWindow!.postMessage(JSON.stringify(msg), '*');
      });

      document.body.appendChild(newwebview);
      // attach the new window to the new <webview>.
      console.info('Step 3: attaches the new webview.');
      const newwindow = (e as Event & {window: Window}).window;
      (newwindow as Window & {
        attach: (frame: HTMLIFrameElement) => void,
      }).attach(newwebview);
    });

    const whenResponseFromChannel1 = new Promise<void>(resolve => {
      window.addEventListener('message', function(e) {
        if (!newwebview || e.source !== newwebview.contentWindow) {
          return;
        }
        const data = JSON.parse(e.data);
        assertEquals(newwebview.contentWindow, e.source);
        assertEquals(
            RESPONSE_FROM_COMM_CHANNEL_1, data[0],
            'Unexpected message: \'' + data[0] + '\'');
        console.info(
            'Step 5: a communication channel has been established ' +
            'with the new webview.');
        resolve();
      });
    });

    console.info('Step 1: navigates the webview to window open guest URL.');
    webview.setAttribute('src', getWebviewUrl());
    document.body.appendChild(webview);
    await whenResponseFromChannel1;
  });

  // Adds a content script to <webview>. This test verifies that the script is
  // injected after terminate and reload <webview>.
  test('ContentScriptIsInjectedAfterTerminateAndReloadWebView', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts.');
    webview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/empty*'],
      js: {files: ['test/webview_execute_script.js']},
      run_at: 'document_end' as chrome.extensionTypes.RunAt,
    }]);

    let count = 0;
    const onGetBackgroundExecuted = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', function() {
        if (count === 0) {
          console.info('Step 2: call webview.terminate().');
          webview.terminate();
          ++count;
          return;
        } else if (count === 1) {
          console.info('Step 4: call <webview>.executeScript to check result.');
          checkBackgroundColor(webview).then(resolve);
        }
      });
    });

    webview.addEventListener('exit', function() {
      console.info('Step 3: call webview.reload().');
      webview.reload();
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await onGetBackgroundExecuted;
  });

  // This test verifies the content script won't be removed when the guest is
  // destroyed, i.e., removed <webview> from the DOM.
  test('ContentScriptExistsAsLongAsWebViewTagExists', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts.');
    webview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/empty*'],
      js: {files: ['test/webview_execute_script.js']},
      run_at: 'document_end' as chrome.extensionTypes.RunAt,
    }]);

    let count = 0;
    const whenBackgroundColorChecked = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', function() {
        if (count === 0) {
          console.info('Step 2: check the result of content script injected.');
          webview.executeScript(
              {code: 'document.body.style.backgroundColor;'},
              function(results) {
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
          checkBackgroundColor(webview).then(resolve);
        }
      });
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await whenBackgroundColorChecked;
  });

  test('AddContentScriptWithCode', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    console.info('Step 1: call <webview>.addContentScripts.');
    webview.addContentScripts([{
      name: 'myrule',
      matches: ['http://*/empty*'],
      js: {code: 'document.body.style.backgroundColor = \'red\';'},
      run_at: 'document_end' as chrome.extensionTypes.RunAt,
    }]);

    const whenLoadStop = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', function() {
        console.info('Step 2: call webview.executeScript() to check result.');
        resolve();
      });
    });

    webview.src = getWebviewUrl();
    document.body.appendChild(webview);
    await whenLoadStop;
    await checkBackgroundColor(webview);
  });

  test('RequestInterceptionCoverageTest', async () => {
    const kResultMessageType = 'TEST_RESULT';
    const kObservedRequestMessageType = 'OBSERVED_REQUEST';
    const kStartTestsMessageType = 'START_TESTS';

    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    const whenLoadStop = new Promise<void>(resolve => {
      webview.addEventListener('loadstop', () => resolve());
    });

    // Promise that resolves when the guest page sends its test results
    // via the transferred port.
    const testResultPromise = whenLoadStop.then(() => {
      const channel = new MessageChannel();
      const resultPromise = new Promise<string>((resolve) => {
        channel.port1.onmessage = (e) => {
          if (e.data.type === kResultMessageType) {
            resolve(e.data.result);
          }
        };
      });
      webview.contentWindow!.postMessage(
          {type: kStartTestsMessageType}, '*', [channel.port2]);
      return resultPromise;
    });

    // Signals to the guest that a request has been observed.
    const signalObservation = (url: string, event: string) => {
      whenLoadStop.then(() => {
        webview.contentWindow!.postMessage(
            {type: kObservedRequestMessageType, data: {url, event}}, '*');
      });
    };


    // Set up request interception for normal and WebSocket requests.
    webview.request.onBeforeRequest.addListener((details: any) => {
      signalObservation(details.url, 'onBeforeRequest');
      return {};
    }, {urls: ['*://*/*', 'ws://*/*']}, ['blocking']);

    // Set up request interception for basic authentication.
    webview.request.onAuthRequired.addListener((details: any) => {
      signalObservation(details.url, 'onAuthRequired');
      return {
        authCredentials: {
          username: 'test',
          password: new URL(details.url).searchParams.get('password')!,
        },
      };
    }, {urls: ['*://*/auth-basic*']}, ['blocking']);

    // Construct guest URL with WebSocket and WebTransport ports.
    const guestUrl = new URL(
        './webview/request_interception_coverage_guest.html', getWebviewUrl());
    guestUrl.searchParams.set('ws_port', getWebSocketPort().toString());
    guestUrl.searchParams.set('wt_port', getWebTransportPort().toString());

    const expectedFailures = [
      {title: 'Service Worker script', event: 'onBeforeRequest'},
      {title: 'Fetch from Shared Worker', event: 'onBeforeRequest'},
      {title: 'Fetch from Service Worker', event: 'onBeforeRequest'},
      {title: 'WebSocket in Shared Worker', event: 'onBeforeRequest'},
      {title: 'WebSocket in Service Worker', event: 'onBeforeRequest'},
      {title: 'WebTransport in Shared Worker', event: 'onBeforeRequest'},
      {title: 'WebTransport in Service Worker', event: 'onBeforeRequest'},
    ];
    guestUrl.searchParams.set(
        'expected_failures', expectedFailures.map(f => f.title).join(','));

    webview.src = guestUrl.href;
    document.body.appendChild(webview);

    const result = await testResultPromise;

    // Expected result indicates that several request types were NOT observed
    // by the <webview> request interception API in this WebUI context.
    const kExpectedResult =
        expectedFailures.map(f => `${f.title}: not observed by ${f.event}`)
            .join('\n');

    assertEquals(
        kExpectedResult, result, `Unexpected test results:\n${result}`);
  });

  test('ExecuteScriptBadUrlFromOtherWebUi', async () => {
    const webview = createWebview();
    let seenError = false;
    webview.addEventListener('consolemessage', () => {
      // If the script runs, it'll log a message that we'll see here.
      seenError = true;
    });

    // It does not make sense to pass an absolute URL to these script injection
    // APIs, especially one that is cross-origin to the embedder. We verify that
    // this does not execute.
    const badUrl = 'chrome://webuijserror/webui_js_error.js';
    webview.addContentScripts([{
      name: 'evil',
      matches: ['http://*/*', 'https://*/*'],
      js: {files: [badUrl]},
      run_at: 'document_start' as chrome.extensionTypes.RunAt,
    }]);

    const loadStopPromise = eventToPromise('loadstop', webview);
    webview.src = getWebviewUrl();
    await loadStopPromise;

    await executeScript(webview, {file: badUrl});

    assertFalse(seenError, 'Script should not have run');
  });
});
