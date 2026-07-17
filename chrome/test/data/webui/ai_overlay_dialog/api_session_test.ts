// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiSession} from 'chrome-untrusted://ai-overlay-dialog/internal/api_session.js';
import type {ApiSessionConfig} from 'chrome-untrusted://ai-overlay-dialog/internal/api_session.js';
import {assertDeepEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeWebSocket {
  onopen: (() => void)|null = null;
  onmessage: ((event: {data: unknown}) => void)|null = null;
  onclose: (() => void)|null = null;
  onerror: ((error: unknown) => void)|null = null;
  readyState: number = WebSocket.CONNECTING;
  sentPayload: unknown = null;

  send(data: string) {
    this.sentPayload = JSON.parse(data);
  }
}

suite('ApiSessionTest', () => {
  test('SendSetupMessageCorrectFormat', () => {
    const fakeWs = new FakeWebSocket();
    const config: ApiSessionConfig = {
      apiKey: 'test-key',
      endpointUrl: 'wss://test.endpoint',
      model: 'models/gemini-2.0-flash-exp',
      voiceName: 'Puck',
      systemInstruction: 'You are a helpful AI assistant.',
      webSocketFactory: () => {
        fakeWs.readyState = WebSocket.OPEN;
        return fakeWs as unknown as WebSocket;
      },
    };

    const session = new ApiSession(config, [], {
      onConnectionChanged: () => {},
      onResponse: () => {},
      onTranscription: () => {},
      onTurnComplete: () => {},
      interrupt: () => {},
      onToolCall: () => Promise.resolve('result'),
    });

    // Exercise ApiSession strictly through its public API
    session.connect();
    assertTrue(fakeWs.onopen !== null);
    fakeWs.onopen();

    assertTrue(fakeWs.sentPayload !== null);
    assertDeepEquals(
        {
          setup: {
            model: 'models/gemini-2.0-flash-exp',
            generation_config: {
              response_modalities: ['AUDIO'],
              speech_config: {
                voice_config: {
                  prebuilt_voice_config: {
                    voice_name: 'Puck',
                  },
                },
              },
            },
            system_instruction: {
              parts: [{
                text: 'You are a helpful AI assistant.',
              }],
            },
            inputAudioTranscription: {},
            outputAudioTranscription: {},
          },
        },
        fakeWs.sentPayload);
  });
});
