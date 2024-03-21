// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onRequest = chrome.declarativeWebRequest.onRequest;
var AddResponseHeader =
    chrome.declarativeWebRequest.AddResponseHeader;
var RequestMatcher = chrome.declarativeWebRequest.RequestMatcher;
var CancelRequest = chrome.declarativeWebRequest.CancelRequest;
var RedirectByRegEx = chrome.declarativeWebRequest.RedirectByRegEx;
var RedirectRequest = chrome.declarativeWebRequest.RedirectRequest;
var RedirectToTransparentImage =
    chrome.declarativeWebRequest.RedirectToTransparentImage;
var RedirectToEmptyDocument =
    chrome.declarativeWebRequest.RedirectToEmptyDocument;
var SetRequestHeader =
    chrome.declarativeWebRequest.SetRequestHeader;
var RemoveRequestHeader =
    chrome.declarativeWebRequest.RemoveRequestHeader;
var RemoveResponseHeader =
    chrome.declarativeWebRequest.RemoveResponseHeader;
var IgnoreRules =
    chrome.declarativeWebRequest.IgnoreRules;
var AddRequestCookie = chrome.declarativeWebRequest.AddRequestCookie;
var AddResponseCookie = chrome.declarativeWebRequest.AddResponseCookie;
var EditRequestCookie = chrome.declarativeWebRequest.EditRequestCookie;
var EditResponseCookie = chrome.declarativeWebRequest.EditResponseCookie;
var RemoveRequestCookie = chrome.declarativeWebRequest.RemoveRequestCookie;
var RemoveResponseCookie = chrome.declarativeWebRequest.RemoveResponseCookie;
var headerName = 'foo';
var headerValue = 'Bar';

// Constants as functions, not to be called until after runTests.
function getURLHttpSimple() {
  return getServerURL("extensions/api_test/webrequest/simpleLoad/a.html");
}

function getURLHttpSimpleB() {
  return getServerURL("extensions/api_test/webrequest/simpleLoad/b.html");
}

function getURLHttpComplex() {
  return getServerURL(
      "extensions/api_test/webrequest/complexLoad/a.html");
}

function getURLHttpRedirectTest() {
  return getServerURL(
      "extensions/api_test/webrequest/declarative/a.html");
}

function getURLHttpWithHeaders() {
  return getServerURL(
      "extensions/api_test/webrequest/declarative/headers.html");
}

function getURLOfHTMLWithThirdParty() {
  // Returns the URL of a HTML document with a third-party resource.
  return getServerURL(
      "extensions/api_test/webrequest/declarative/third-party.html");
}

function getURLEchoUserAgent() {
  return getServerURL('echoheader?User-Agent');
}

function getURLHttpSimple() {
  return getServerURL("extensions/api_test/webrequest/simpleLoad/a.html");
}

function getURLSetHeader() {
  return getServerURL('set-header?' + headerName + ': ' + headerValue);
}

function getURLSetCookie2() {
  return getServerURL('set-cookie?passedCookie=Foo&editedCookie=Foo&' +
                      'deletedCookie=Foo');
}

function getURLEchoCookie() {
  return getServerURL('echoheader?Cookie');
}

// Shared test sections.
function cancelThirdPartyExpected() {
    return [
      { label: "onBeforeRequest",
        event: "onBeforeRequest",
        details: {
          url: getURLOfHTMLWithThirdParty(),
          frameUrl: getURLOfHTMLWithThirdParty(),
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "onBeforeSendHeaders",
        event: "onBeforeSendHeaders",
        details: {
          url: getURLOfHTMLWithThirdParty(),
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "onSendHeaders",
        event: "onSendHeaders",
        details: {
          url: getURLOfHTMLWithThirdParty(),
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "onHeadersReceived",
        event: "onHeadersReceived",
        details: {
          url: getURLOfHTMLWithThirdParty(),
          statusLine: "HTTP/1.1 200 OK",
          statusCode: 200,
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "onResponseStarted",
        event: "onResponseStarted",
        details: {
          url: getURLOfHTMLWithThirdParty(),
          fromCache: false,
          ip: "127.0.0.1",
          statusCode: 200,
          statusLine: "HTTP/1.1 200 OK",
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "onCompleted",
        event: "onCompleted",
        details: {
          fromCache: false,
          ip: "127.0.0.1",
          url: getURLOfHTMLWithThirdParty(),
          statusCode: 200,
          statusLine: "HTTP/1.1 200 OK",
          initiator: getServerDomain(initiators.BROWSER_INITIATED)
        }
      },
      { label: "img-onBeforeRequest",
        event: "onBeforeRequest",
        details: {
          type: "image",
          url: "http://non_existing_third_party.com/image.png",
          frameUrl: getURLOfHTMLWithThirdParty(),
          initiator: getServerDomain(initiators.WEB_INITIATED)
        }
      },
      { label: "img-onErrorOccurred",
        event: "onErrorOccurred",
        details: {
          error: "net::ERR_BLOCKED_BY_CLIENT",
          fromCache: false,
          type: "image",
          url: "http://non_existing_third_party.com/image.png",
          initiator: getServerDomain(initiators.WEB_INITIATED)
        }
      },
    ];
}

function cancelThirdPartyExpectedOrder() {
    return [
      ["onBeforeRequest", "onBeforeSendHeaders", "onSendHeaders",
       "onHeadersReceived", "onResponseStarted", "onCompleted"],
      ["img-onBeforeRequest", "img-onErrorOccurred"]
    ];
}

let allTests = [

  function testCancelRequest() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpWithHeaders(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onErrorOccurred"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({
             'url': {
                 'pathSuffix': ".html",
                 'ports': [testServerPort, [1000, 2000]],
                 'schemes': ["http"]
             },
             'resourceType': ["main_frame"],
             'contentType': ["text/plain"],
             'excludeContentType': ["image/png"],
             'responseHeaders': [{ nameContains: ["content", "type"] }],
             'excludeResponseHeaders': [{ valueContains: "nonsense" }],
             'stages': ["onHeadersReceived", "onAuthRequired"] })],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpWithHeaders());}
    );
  },

  // Postpone cancelling of the request until onHeadersReceived by using
  // 'stages'. If not for the stages, the request would be already cancelled
  // during onBeforeRequest.
  function testPostponeCancelRequest() {
    ignoreUnexpected = false;
    expect(
      [
        { label: "onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURLHttpWithHeaders(),
            frameUrl: getURLHttpWithHeaders()
          }
        },
        { label: "onBeforeSendHeaders",
          event: "onBeforeSendHeaders",
          details: {
            url: getURLHttpWithHeaders()
          }
        },
        { label: "onSendHeaders",
          event: "onSendHeaders",
          details: {
            url: getURLHttpWithHeaders()
          }
        },
        { label: "onHeadersReceived",
          event: "onHeadersReceived",
          details: {
            statusLine: "HTTP/1.1 200 OK",
            url: getURLHttpWithHeaders(),
            statusCode: 200
          }
        },
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpWithHeaders(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT"
          }
        },
      ],
      [ ["onBeforeRequest", "onBeforeSendHeaders", "onSendHeaders",
         "onHeadersReceived", "onErrorOccurred"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({ 'stages': ["onHeadersReceived"] })],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpWithHeaders());}
    );
  },
  // Tests that "thirdPartyForCookies: true" matches third party requests.
  function testThirdParty() {
    ignoreUnexpected = false;
    expect(cancelThirdPartyExpected(), cancelThirdPartyExpectedOrder());
    onRequest.addRules(
      [ {'conditions': [new RequestMatcher({thirdPartyForCookies: true})],
         'actions': [new chrome.declarativeWebRequest.CancelRequest()]},],
      function() {navigateAndWait(getURLOfHTMLWithThirdParty());}
    );
  },

  // Tests that "thirdPartyForCookies: false" matches first party requests,
  // by cancelling all requests, and overriding the cancelling rule only for
  // requests matching "thirdPartyForCookies: false".
  function testFirstParty() {
    ignoreUnexpected = false;
    expect(cancelThirdPartyExpected(), cancelThirdPartyExpectedOrder());
    onRequest.addRules(
      [ {'priority': 2,
         'conditions': [
           new RequestMatcher({thirdPartyForCookies: false})
         ],
         'actions': [
           new chrome.declarativeWebRequest.IgnoreRules({
              lowerPriorityThan: 2 })
         ]
        },
        {'priority': 1,
         'conditions': [new RequestMatcher({})],
         'actions': [new chrome.declarativeWebRequest.CancelRequest()]
        },
      ],
      function() {navigateAndWait(getURLOfHTMLWithThirdParty());}
    );
  },

  function testSiteForCookiesUrl() {
    // This is an end-to-end test for firstPartyForCookies being ignored,
    // so the cancellation matches.
    ignoreUnexpected = false;
    expect(
      [
        { label: "onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURLOfHTMLWithThirdParty(),
            frameUrl: getURLOfHTMLWithThirdParty(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLOfHTMLWithThirdParty(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onBeforeRequest", "onErrorOccurred"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({
             firstPartyForCookiesUrl: {
               hostEquals: "not" + testServer
             }
           })
         ],
         'actions': [new chrome.declarativeWebRequest.CancelRequest()]
        },
      ],
      function() {navigateAndWait(getURLOfHTMLWithThirdParty());}
    );
  },

  function testRedirectRequest() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onBeforeRequest-a",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpComplex(),
            frameUrl: getURLHttpComplex(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
        },
        { label: "onBeforeRedirect",
          event: "onBeforeRedirect",
          details: {
            url: getURLHttpComplex(),
            redirectUrl: getURLHttpSimple(),
            fromCache: false,
            statusLine: "HTTP/1.1 307 Internal Redirect",
            statusCode: 307,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "onBeforeRequest-b",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpSimple(),
            frameUrl: getURLHttpSimple(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
        },
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpSimple(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onBeforeRequest-a", "onBeforeRedirect", "onBeforeRequest-b",
         "onCompleted"] ]);

    onRequest.addRules(
      [ {'conditions': [new RequestMatcher({'url': {'pathSuffix': ".html"}})],
         'actions': [
             new RedirectRequest({'redirectUrl': getURLHttpSimple()})]}
      ],
      function() {navigateAndWait(getURLHttpComplex());}
    );
  },

  function testRedirectRequest2() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpRedirectTest(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        // We cannot wait for onCompleted signals because these are not sent
        // for data:// URLs.
        { label: "onBeforeRedirect-1",
          event: "onBeforeRedirect",
          details: {
            url: getServerURL(
                "extensions/api_test/webrequest/declarative/image.png"),
            redirectUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEA" +
                "AAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJ" +
                "ggg==",
            fromCache: false,
            statusLine: "HTTP/1.1 307 Internal Redirect",
            statusCode: 307,
            type: "image",
            initiator: getServerDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "onBeforeRedirect-2",
          event: "onBeforeRedirect",
          details: {
            frameId: 1,
            parentFrameId: 0,
            url: getServerURL(
                "extensions/api_test/webrequest/declarative/frame.html"),
            redirectUrl: "data:text/html,",
            fromCache: false,
            statusLine: "HTTP/1.1 307 Internal Redirect",
            statusCode: 307,
            type: "sub_frame",
            initiator: getServerDomain(initiators.WEB_INITIATED)
          }
        },
      ],
      [ ["onCompleted"], ["onBeforeRedirect-1"], ["onBeforeRedirect-2"] ]);

    onRequest.addRules(
      [ {conditions: [
             new RequestMatcher({url: {pathSuffix: "image.png"}})],
         actions: [new RedirectToTransparentImage()]},
        {conditions: [
             new RequestMatcher({url: {pathSuffix: "frame.html"}})],
         actions: [new RedirectToEmptyDocument()]},
      ],
      function() {navigateAndWait(getURLHttpRedirectTest());}
    );
  },

  // Tests that a request is redirected during the onHeadersReceived stage
  // when the conditions include a RequestMatcher with a contentType.
  function testRedirectRequestByContentType() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onBeforeRequest-a",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpWithHeaders(),
            frameUrl: getURLHttpWithHeaders(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
        },
        { label: "onBeforeRedirect",
          event: "onBeforeRedirect",
          details: {
            url: getURLHttpWithHeaders(),
            redirectUrl: getURLHttpSimple(),
            statusLine: "HTTP/1.1 302 Found",
            statusCode: 302,
            fromCache: false,
            ip: "127.0.0.1",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "onBeforeRequest-b",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpSimple(),
            frameUrl: getURLHttpSimple(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
        },
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpSimple(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onBeforeRequest-a", "onBeforeRedirect", "onBeforeRequest-b",
         "onCompleted"] ]);

    onRequest.addRules(
      [ {'conditions': [new RequestMatcher({'contentType': ["text/plain"]})],
         'actions': [
             new RedirectRequest({'redirectUrl': getURLHttpSimple()})]}
      ],
      function() {navigateAndWait(getURLHttpWithHeaders());}
    );
  },

  function testRedirectByRegEx() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpSimpleB(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onCompleted"] ]);

    onRequest.addRules(
      [ {conditions: [new RequestMatcher({url: {pathSuffix: ".html"}})],
         actions: [
             new RedirectByRegEx({from: "^(.*)/a.html$", to: "$1/b.html"})]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },

  function testRegexFilter() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpSimple(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onErrorOccurred"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({
             'url': {
                 'urlMatches': 'simple[A-Z].*a\.html$',
                 'schemes': ["http"]
             },
           })],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },
  function testSetRequestHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new SetRequestHeader({name: "User-Agent", value: "FoobarUA"})]
       }],
      function() {
        // Check the page content for our modified User-Agent string.
        navigateAndWait(getURLEchoUserAgent(), function() {
          chrome.test.listenOnce(chrome.runtime.onMessage, function(request) {
            chrome.test.assertTrue(request.pass, "Request header was not set.");
          });
          chrome.tabs.executeScript(tabId,
            {
              code: "chrome.runtime.sendMessage(" +
                    "{pass: document.body.innerText.indexOf('FoobarUA') >= 0});"
            });
        });
      });
  },

  function testRemoveRequestHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new RemoveRequestHeader({name: headerName})]
       }],
      chrome.test.callbackPass(function() {
        passCallback = chrome.test.callbackPass((response) => {
          chrome.test.assertEq(undefined, response.headers.get(headerName));
        });
        fetch(getServerURL('echoheader?' + headerName)).then((response) => {
          passCallback(response);
        }).catch((e) => {
          chrome.test.fail(e);
        });
      }));
  },

  function testAddResponseHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new AddResponseHeader({name: headerName, value: headerValue})]
       }],
      chrome.test.callbackPass(function() {
        passCallback = chrome.test.callbackPass((response) => {
          chrome.test.assertEq(headerValue, response.headers.get(headerName));
        });
        fetch(getServerURL('echo')).then((response) => {
          passCallback(response);
        }).catch((e) => {
          chrome.test.fail(e);
        });
      }));
  },

  function testRemoveResponseHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new RemoveResponseHeader({name: headerName,
                                            value: headerValue})]
       }],
      chrome.test.callbackPass(function() {
        passCallback = chrome.test.callbackPass((response) => {
          chrome.test.assertEq(undefined, response.headers.get(headerName));
        });
        fetch(getURLSetHeader()).then((response) => {
          passCallback(response);
        }).catch((e) => {
          chrome.test.fail(e);
        });
      }));
  },

  function testPriorities() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            url: getURLHttpSimple(),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            ip: "127.0.0.1",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        }
      ],
      [ ["onCompleted"] ]);

    onRequest.addRules(
      [ {conditions: [new RequestMatcher({url: {pathContains: "simpleLoad"}})],
         actions: [new CancelRequest()]},
        {conditions: [new RequestMatcher({url: {pathContains: "a.html"}})],
         actions: [new IgnoreRules({lowerPriorityThan: 200})],
         priority: 200}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },

  function testEditRequestCookies() {
    ignoreUnexpected = true;
    expect();
    var cookie1 = {name: "requestCookie1", value: "foo"};
    var cookie2 = {name: "requestCookie2", value: "foo"};
    onRequest.addRules(
      [ {conditions: [new RequestMatcher({})],
         actions: [
           // We exploit the fact that cookies are first added, then modified
           // and finally removed.
           new AddRequestCookie({cookie: cookie1}),
           new AddRequestCookie({cookie: cookie2}),
           new EditRequestCookie({filter: {name: "requestCookie1"},
                                  modification: {value: "bar"}}),
           new RemoveRequestCookie({filter: {name: "requestCookie2"}})
         ]}
      ],
      function() {
        navigateAndWait(getURLEchoCookie(), function() {
          chrome.test.listenOnce(chrome.runtime.onMessage, function(request) {
            chrome.test.assertTrue(request.pass, "Invalid cookies. " +
                JSON.stringify(request.cookies));
          });
          chrome.tabs.executeScript(tabId, {code:
              "function hasCookie(name, value) {" +
              "  var entry = name + '=' + value;" +
              "  return document.body.innerText.indexOf(entry) >= 0;" +
              "};" +
              "var result = {};" +
              "result.pass = hasCookie('requestCookie1', 'bar') && " +
              "              !hasCookie('requestCookie1', 'foo') && " +
              "              !hasCookie('requestCookie2', 'foo');" +
              "result.cookies = document.body.innerText;" +
              "chrome.runtime.sendMessage(result);"});
        });
      }
    );
  },

  function testRequestHeaders() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpSimple(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [ ["onErrorOccurred"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({
             'url': {
                 'pathSuffix': ".html",
                 'ports': [testServerPort, [1000, 2000]],
                 'schemes': ["http"]
             },
             'requestHeaders': [{ nameContains: "" }],
             'excludeRequestHeaders': [{ valueContains: ["", "value123"] }]
              })],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },
];

// All tests in the first suite that are currently working. There are
// two suites to keep the run time of the test fixture down to avoid
// timeouts.
let workingTests1 = [
  'testCancelRequest',
  'testPostponeCancelRequest',
  'testSiteForCookiesUrl',
  'testRedirectRequest',
  'testRedirectRequestByContentType',
  'testRedirectByRegEx',
  'testRegexFilter',
];

// All tests in the second suite that are currently working.
let workingTests2 = [
  'testSetRequestHeader',
  'testRemoveRequestHeader',
  'testAddResponseHeader',
  'testRemoveResponseHeader',
  'testPriorities',
  'testEditRequestCookies',
  'testRequestHeaders',
];

// All tests that are broken or flaky. See https://crbug.com/846555.
let brokenTests = [
  'testThirdParty',  // Generates unexpected events.
  'testFirstParty',  // Generates unexpected events,
  'testRedirectRequest2',  // Hangs.
];

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.test.getConfig(function(config) {
    let args = JSON.parse(config.customArg);
    if (args.testSuite == 'normal1') {
      runTests(allTests.filter(function(op) {
        return workingTests1.includes(op.name);
      }));
    } else if (args.testSuite == 'normal2') {
      runTests(allTests.filter(function(op) {
        return workingTests2.includes(op.name);
      }));
    } else {
      chrome.test.assertEq('broken', args.testSuite);
      runTests(allTests.filter(function(op) {
        return brokenTests.includes(op.name);
      }));
    }
  })
});
