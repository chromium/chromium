// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ClientApi, SessionConfig} from 'chrome-untrusted://boca-app/app/boca_app.js';
import {UrlType} from 'chrome-untrusted://boca-app/app/boca_app.js';
import {callbackRouter} from 'chrome-untrusted://boca-app/app/mojo_api_bootstrap.js';
import type {ConfigResult} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import {UrlType as UrlTypeMojo} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import {assertDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

/**
 * Waits for the boca-app element to be added to the DOM.
 */
async function waitForApp(): Promise<ClientApi> {
  const app = document.querySelector('boca-app');
  if (app) {
    return app as unknown as ClientApi;
  }
  return new Promise(resolve => {
    const observer = new MutationObserver((_, obs) => {
      const app = document.querySelector('boca-app');
      if (app) {
        obs.disconnect();
        resolve(app as unknown as ClientApi);
      }
    });
    observer.observe(document.body, {childList: true});
  });
}

suite('ReceiverTest', function() {
  let app: ClientApi;

  const urlTypeTestCases = [
    {
      name: 'Gemini Regular',
      urlTypeMojo: UrlTypeMojo.kGeminiRegular,
      urlTypeUi: UrlType.GEMINI_REGULAR,
    },
    {
      name: 'Gemini Guided Learning',
      urlTypeMojo: UrlTypeMojo.kGeminiGuidedLearning,
      urlTypeUi: UrlType.GEMINI_GUIDED_LEARNING,
    },
  ];

  setup(async () => {
    app = await waitForApp();
  });

  urlTypeTestCases.forEach((testCase) => {
    test(
        `should correctly update session config with urlType: ${testCase.name}`,
        async () => {
          let actualConfig: SessionConfig|null = null;
          app.onSessionConfigUpdated = (config: SessionConfig|null) => {
            actualConfig = config;
          };

          const sessionConfig: ConfigResult = {
            config: {
              sessionDuration: {
                microseconds: 120000000n,
              },
              sessionStartTime: null,
              teacher: null,
              students: [],
              studentsJoinViaCode: [],
              accessCode: null,
              captionConfig: {
                sessionCaptionEnabled: true,
                localCaptionEnabled: true,
                sessionTranslationEnabled: true,
              },
              onTaskConfig: {
                isLocked: true,
                isPaused: true,
                tabs: [
                  {
                    tab: {
                      id: 1,
                      title: 'special url',
                      url: 'http://specialurl.com/',
                      favicon: 'data/image',
                      urlType: testCase.urlTypeMojo,
                    },
                    navigationType: 0,
                  },
                  {
                    tab: {
                      id: 2,
                      title: 'regular url',
                      url: 'http://regularurl.com/',
                      favicon: 'data/image',
                      urlType: null,
                    },
                    navigationType: 1,
                  },
                ],
              },
            },
          };

          const callbackRouterRemote =
              callbackRouter.$.bindNewPipeAndPassRemote();
          callbackRouterRemote.onSessionConfigUpdated(sessionConfig);
          await callbackRouterRemote.$.flushForTesting();

          assertDeepEquals(
              {
                sessionDurationInMinutes: 2,
                sessionStartTime: undefined,
                teacher: undefined,
                students: [],
                studentsJoinViaCode: [],
                accessCode: '',
                captionConfig: {
                  sessionCaptionEnabled: true,
                  localCaptionEnabled: true,
                  sessionTranslationEnabled: true,
                },
                onTaskConfig: {
                  isLocked: true,
                  isPaused: true,
                  tabs: [
                    {
                      tab: {
                        id: 1,
                        title: 'special url',
                        url: 'http://specialurl.com/',
                        favicon: 'data/image',
                        urlType: testCase.urlTypeUi,
                      },
                      navigationType: 0,
                    },
                    {
                      tab: {
                        id: 2,
                        title: 'regular url',
                        url: 'http://regularurl.com/',
                        favicon: 'data/image',
                        urlType: undefined,
                      },
                      navigationType: 1,
                    },
                  ],
                },
              },
              actualConfig);
        });
  });
});
