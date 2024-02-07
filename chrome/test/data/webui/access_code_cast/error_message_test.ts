// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/error_message/error_message.js';

import {AddSinkResultCode} from 'chrome://access-code-cast/access_code_cast.mojom-webui.js';
import type {ErrorMessageElement} from 'chrome://access-code-cast/error_message/error_message.js';
import {RouteRequestResultCode} from 'chrome://access-code-cast/route_request_result_code.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ErrorMessageElementTest', () => {
  let c2cErrorMessage: ErrorMessageElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    c2cErrorMessage = document.createElement('c2c-error-message');
    document.body.appendChild(c2cErrorMessage);
  });

  test('setAddSinkError', () => {
    c2cErrorMessage.setNoError();

    const testValues = [
      {addResult: AddSinkResultCode.UNKNOWN_ERROR, expectedMessage: 1},
      {addResult: AddSinkResultCode.OK, expectedMessage: 0},
      {addResult: AddSinkResultCode.AUTH_ERROR, expectedMessage: 4},
      {
        addResult: AddSinkResultCode.HTTP_RESPONSE_CODE_ERROR,
        expectedMessage: 3,
      },
      {addResult: AddSinkResultCode.RESPONSE_MALFORMED, expectedMessage: 3},
      {addResult: AddSinkResultCode.EMPTY_RESPONSE, expectedMessage: 3},
      {addResult: AddSinkResultCode.INVALID_ACCESS_CODE, expectedMessage: 2},
      {addResult: AddSinkResultCode.ACCESS_CODE_NOT_FOUND, expectedMessage: 2},
      {addResult: AddSinkResultCode.TOO_MANY_REQUESTS, expectedMessage: 5},
      {addResult: AddSinkResultCode.SERVICE_NOT_PRESENT, expectedMessage: 3},
      {addResult: AddSinkResultCode.SERVER_ERROR, expectedMessage: 3},
      {addResult: AddSinkResultCode.SINK_CREATION_ERROR, expectedMessage: 1},
      {addResult: AddSinkResultCode.CHANNEL_OPEN_ERROR, expectedMessage: 7},
      {addResult: AddSinkResultCode.PROFILE_SYNC_ERROR, expectedMessage: 6},
      {
        addResult: AddSinkResultCode.INTERNAL_MEDIA_ROUTER_ERROR,
        expectedMessage: 1,
      },
    ];

    for (let i = 0; i < testValues.length; i++) {
      c2cErrorMessage.setAddSinkError(testValues[i]!.addResult);
      assertEquals(testValues[i]!.expectedMessage,
        c2cErrorMessage.getMessageCode());
      c2cErrorMessage.setNoError();
    }
  });

  test('setCastError', () => {
    c2cErrorMessage.setNoError();

    const testValues = [
      {castResult: RouteRequestResultCode.UNKNOWN_ERROR, expectedMessage: 1},
      {castResult: RouteRequestResultCode.OK, expectedMessage: 0},
      {castResult: RouteRequestResultCode.TIMED_OUT, expectedMessage: 3},
      {castResult: RouteRequestResultCode.ROUTE_NOT_FOUND, expectedMessage: 3},
      {castResult: RouteRequestResultCode.SINK_NOT_FOUND, expectedMessage: 3},
      {castResult: RouteRequestResultCode.INVALID_ORIGIN, expectedMessage: 1},
      {
        castResult: RouteRequestResultCode.DEPRECATED_OFF_THE_RECORD_MISMATCH,
        expectedMessage: 1,
      },
      {
        castResult: RouteRequestResultCode.NO_SUPPORTED_PROVIDER,
        expectedMessage: 1,
      },
      {castResult: RouteRequestResultCode.CANCELLED, expectedMessage: 1},
      {
        castResult: RouteRequestResultCode.ROUTE_ALREADY_EXISTS,
        expectedMessage: 1,
      },
      {
        castResult: RouteRequestResultCode.DESKTOP_PICKER_FAILED,
        expectedMessage: 1,
      },
      {
        castResult: RouteRequestResultCode.ROUTE_ALREADY_TERMINATED,
        expectedMessage: 1,
      },
    ];

    for (let i = 0; i < testValues.length; i++) {
      c2cErrorMessage.setCastError(testValues[i]!.castResult);
      assertEquals(testValues[i]!.expectedMessage,
        c2cErrorMessage.getMessageCode());
      c2cErrorMessage.setNoError();
    }
  });
});
